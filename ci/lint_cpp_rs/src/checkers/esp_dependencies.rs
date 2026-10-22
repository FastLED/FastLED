// Native ESP include closure. Repository directives are inspected in every
// branch; this is deliberately not a claim to preprocess arbitrary C++.
#[derive(Clone, PartialEq, Eq, PartialOrd, Ord)]
struct EspIncludePath {
    path: PathBuf,
    quoted_only: bool,
    system: bool,
}

fn esp_dependency_include_dirs(root: &Path) -> Vec<Vec<EspIncludePath>> {
    let mut databases = vec![root.join("compile_commands.json")];
    if let Ok(builds) = fs::read_dir(root.join(".build/fbuild")) {
        databases.extend(
            builds
                .flatten()
                .map(|entry| entry.path().join("compile_commands.json")),
        );
    }
    databases.sort();
    let mut profiles = BTreeSet::new();
    profiles.insert(vec![EspIncludePath {
        path: root.join("src"),
        quoted_only: false,
        system: false,
    }]);
    for database in databases {
        let Ok(text) = fs::read_to_string(&database) else {
            continue;
        };
        let Ok(entries) = serde_json::from_str::<serde_json::Value>(&text) else {
            continue;
        };
        let Some(entries) = entries.as_array() else {
            continue;
        };
        for entry in entries {
            let arguments: Vec<String> = if let Some(arguments) =
                entry.get("arguments").and_then(|value| value.as_array())
            {
                arguments
                    .iter()
                    .filter_map(|value| value.as_str().map(str::to_owned))
                    .collect()
            } else if let Some(command) = entry.get("command").and_then(|value| value.as_str()) {
                let Some(arguments) = shlex::split(command) else {
                    continue;
                };
                arguments
            } else {
                continue;
            };
            if !arguments.iter().any(|arg| {
                arg.starts_with("-DESP32")
                    || arg.starts_with("-DARDUINO_ARCH_ESP32")
                    || arg.starts_with("-DESP_PLATFORM")
                    || arg.starts_with("-DCONFIG_IDF_TARGET_")
            }) {
                continue;
            }
            let directory = entry
                .get("directory")
                .and_then(|value| value.as_str())
                .map(PathBuf::from)
                .unwrap_or_else(|| root.to_path_buf());
            let mut dirs = Vec::new();
            let args: Vec<&str> = arguments.iter().map(String::as_str).collect();
            let mut index = 0;
            while index < args.len() {
                let arg = args[index];
                let include = if ["-I", "-isystem", "-iquote"].contains(&arg) {
                    index += 1;
                    args.get(index).copied()
                } else {
                    arg.strip_prefix("-I").filter(|value| !value.is_empty())
                };
                if let Some(include) = include {
                    let path = PathBuf::from(include);
                    let path = if path.is_absolute() {
                        path
                    } else {
                        directory.join(path)
                    };
                    // Inspect current sources in place of fbuild's staged copy,
                    // retaining its position in the compiler's search order.
                    let staged = path.strip_prefix(root.join(".build/fbuild")).ok();
                    let path = if let Some(staged) = staged {
                        let parts: Vec<_> = staged.components().collect();
                        if parts.len() >= 3
                            && parts[1].as_os_str() == "lib"
                            && parts[2].as_os_str() == "FastLED"
                        {
                            root.join("src")
                                .join(parts[3..].iter().collect::<PathBuf>())
                        } else {
                            path
                        }
                    } else {
                        path
                    };
                    let include = EspIncludePath {
                        path,
                        quoted_only: arg == "-iquote",
                        system: arg == "-isystem",
                    };
                    if include.path.is_dir() && !dirs.contains(&include) {
                        dirs.push(include);
                    }
                }
                index += 1;
            }
            // Compiler include classes override argv interleaving. Stable
            // sorting retains each class's user-supplied directory order.
            dirs.sort_by_key(|dir| {
                if dir.quoted_only {
                    0
                } else if dir.system {
                    2
                } else {
                    1
                }
            });
            profiles.insert(dirs);
        }
    }
    profiles.into_iter().collect()
}

fn regex_esp_dependency_literal() -> &'static Regex {
    static INCLUDE: OnceLock<Regex> = OnceLock::new();
    INCLUDE.get_or_init(|| Regex::new(r#"^\s*#\s*include\s*([<"])([^>"]+)[>"]"#).unwrap())
}

fn esp_dependency_includes(content: &FileContent) -> Vec<(usize, String, bool)> {
    esp_native_directives(content)
        .into_iter()
        .filter_map(|(line, directive)| {
            regex_esp_dependency_literal()
                .captures(&directive)
                .map(|capture| (line, capture[2].to_string(), &capture[1] == "\""))
        })
        .collect()
}

fn esp_dependency_unresolved(content: &FileContent) -> Option<(usize, String)> {
    static INCLUDE: OnceLock<Regex> = OnceLock::new();
    let include = INCLUDE.get_or_init(|| Regex::new(r"^\s*#\s*include\b").unwrap());
    for (line, directive) in esp_native_directives(content) {
        if include.is_match(&directive) && !regex_esp_dependency_literal().is_match(&directive) {
            return Some((line, "unresolved macro-generated include; use a literal native SDK header so lint can establish the dependency boundary".to_string()));
        }
    }
    for (line, directive) in esp_native_directives_with_conditions(content, true) {
        if regex_esp_arduino_probe()
            .find_iter(&directive)
            .any(|hit| esp_arduino_probe_is_code(&directive, hit.start()))
        {
            return Some((
                line,
                "Arduino header availability probe reached through a native dependency".to_string(),
            ));
        }
    }
    None
}

fn esp_dependency_read(
    path: &Path,
    cache: &mut HashMap<PathBuf, std::sync::Arc<FileContent>>,
) -> Option<std::sync::Arc<FileContent>> {
    if let Some(content) = cache.get(path) {
        return Some(std::sync::Arc::clone(content));
    }
    let content = std::sync::Arc::new(FileContent::read(path).ok()?);
    cache.insert(path.to_path_buf(), std::sync::Arc::clone(&content));
    Some(content)
}

fn esp_dependency_forbidden(header: &str) -> bool {
    let directive = format!("#include \"{header}\"");
    regex_esp_arduino_include().is_match(&directive)
        || Path::new(header)
            .file_name()
            .and_then(|name| name.to_str())
            .is_some_and(|name| {
                name.eq_ignore_ascii_case("Arduino.h") || name.eq_ignore_ascii_case("WProgram.h")
            })
}

const ESP_LEGACY_SPI_SHIM: &str = "src/platforms/esp/32/core/fastspi_esp32_arduino.h";
const ESP_LEGACY_SPI_ADAPTER: &str = "platforms/arduino/spi/fastspi_esp32_arduino.h";

fn esp_legacy_spi_shim_valid(content: &FileContent) -> bool {
    let directives: Vec<String> = esp_preprocessor_directives(content)
        .into_iter()
        .map(|(_, text)| text.split_whitespace().collect::<String>())
        .collect();
    directives
        == [
            "#pragmaonce".to_string(),
            format!("#include\"{ESP_LEGACY_SPI_ADAPTER}\""),
        ]
}

fn esp_legacy_spi_opt_in(content: &FileContent, line: usize) -> bool {
    let mut guards = Vec::new();
    for (directive_line, text) in esp_preprocessor_directives(content) {
        if directive_line == line {
            return guards.contains(&true);
        }
        let code = text.split_whitespace().collect::<String>();
        if code.starts_with("#if") {
            guards.push(
                code == "#ifdefined(FL_ESP32_SPI_ARDUINO)&&FL_ESP32_SPI_ARDUINO&&defined(ARDUINO)",
            );
        } else if code.starts_with("#elif") || code == "#else" {
            if let Some(guard) = guards.last_mut() {
                *guard = false;
            }
        } else if code == "#endif" {
            guards.pop();
        }
    }
    false
}

fn esp_arduino_dependency_pass(
    root: &Path,
    cache: &mut HashMap<PathBuf, std::sync::Arc<FileContent>>,
) -> Vec<LintViolation> {
    let mut roots: Vec<PathBuf> = cache
        .keys()
        .filter(|path| {
            path.strip_prefix(root).ok().is_some_and(|relative| {
                let name = normalize_path(&path_to_string(relative));
                name.starts_with("src/platforms/esp/")
                    && !name.starts_with("src/platforms/esp/8266/")
                    && name != ESP_LEGACY_SPI_SHIM
            })
        })
        .cloned()
        .collect();
    roots.sort();
    let mut parsed = HashMap::<PathBuf, Vec<(usize, String, bool)>>::new();
    let mut unresolved = HashMap::<PathBuf, Option<(usize, String)>>::new();
    let mut violations = BTreeSet::new();
    if let Some(shim) = esp_dependency_read(&root.join(ESP_LEGACY_SPI_SHIM), cache) {
        if !esp_legacy_spi_shim_valid(&shim) {
            violations.insert(LintViolation { checker: "EspArduinoHeaderChecker".to_string(),
                path: ESP_LEGACY_SPI_SHIM.to_string(), line: 1,
                message: "The legacy Arduino SPI compatibility path must remain a pure forwarding shim; place implementation in the explicit Arduino adapter".to_string() });
        }
    }
    for dirs in esp_dependency_include_dirs(root) {
        let mut pending: std::collections::VecDeque<PathBuf> = roots.iter().cloned().collect();
        let mut visited = HashSet::new();
        let mut reverse = HashMap::<PathBuf, Vec<(PathBuf, usize, String)>>::new();
        let mut witnesses = BTreeMap::<PathBuf, (usize, String)>::new();
        let mut external_count = 0;
        while let Some(path) = pending.pop_front() {
            if !visited.insert(path.clone()) {
                continue;
            }
            if !path.starts_with(root.join("src")) {
                external_count += 1;
                if external_count > 4096 {
                    witnesses.insert(path, (1, "include closure exceeded 4096 reachable SDK headers; narrow this dependency".to_string()));
                    continue;
                }
            }
            let includes = if let Some(includes) = parsed.get(&path) {
                includes.clone()
            } else {
                let Some(content) = esp_dependency_read(&path, cache) else {
                    continue;
                };
                let includes = esp_dependency_includes(&content);
                unresolved.insert(path.clone(), esp_dependency_unresolved(&content));
                parsed.insert(path.clone(), includes.clone());
                includes
            };
            if path.starts_with(root) {
                if let Some(Some((line, problem))) = unresolved.get(&path) {
                    let display = path
                        .strip_prefix(root)
                        .unwrap_or(&path)
                        .display()
                        .to_string();
                    witnesses
                        .entry(path.clone())
                        .or_insert((*line, format!("{display}:{line}: {problem}")));
                }
            }
            for (line, header, quoted) in includes {
                let display = path
                    .strip_prefix(root)
                    .unwrap_or(&path)
                    .display()
                    .to_string();
                let edge = format!("{display}:{line} -> {header}");
                let shim_header = ESP_LEGACY_SPI_SHIM.trim_start_matches("src/");
                let adapter_edge =
                    header == shim_header || header.starts_with("platforms/arduino/");
                if adapter_edge {
                    let source_name = path.strip_prefix(root).unwrap_or(&path);
                    if source_name == Path::new("src/platforms/esp/32/core/fastspi_esp32.h")
                        && header == shim_header
                        && esp_dependency_read(&path, cache)
                            .is_some_and(|content| esp_legacy_spi_opt_in(&content, line))
                    {
                        continue;
                    }
                    witnesses.entry(path.clone()).or_insert((
                        line,
                        format!("{edge} (Arduino adapter reached from native implementation)"),
                    ));
                    continue;
                }
                if esp_dependency_forbidden(&header) {
                    witnesses.entry(path.clone()).or_insert((line, edge));
                    continue;
                }
                let candidates = path
                    .parent()
                    .into_iter()
                    .filter(|_| quoted)
                    .map(|parent| parent.join(&header))
                    .chain(
                        dirs.iter()
                            .filter(|dir| quoted || !dir.quoted_only)
                            .map(|dir| dir.path.join(&header)),
                    );
                if let Some(resolved) = candidates
                    .filter(|path| path.is_file())
                    .find_map(|path| path.canonicalize().ok())
                {
                    if resolved.starts_with(root.join("src/platforms/arduino")) {
                        witnesses.entry(path.clone()).or_insert((
                            line,
                            format!("{edge} (resolved application-owned Arduino adapter)"),
                        ));
                        continue;
                    }
                    reverse
                        .entry(resolved.clone())
                        .or_default()
                        .push((path.clone(), line, header));
                    pending.push_back(resolved);
                } else if ["FS.h", "LittleFS.h", "SPI.h", "Wire.h", "WiFi.h"]
                    .contains(&header.as_str())
                {
                    witnesses.entry(path.clone()).or_insert((line, format!("{edge} (Arduino-owned interface; FS.h includes Arduino.h in supported cores). Move this dependency into an application-owned adapter; missing SDK metadata is not an exemption")));
                }
            }
        }
        // Reverse breadth-first propagation handles cycles without caching an
        // incomplete negative result, and records one short witness per node.
        let mut pending: std::collections::VecDeque<PathBuf> = witnesses.keys().cloned().collect();
        while let Some(child) = pending.pop_front() {
            let message = witnesses[&child].1.clone();
            for (parent, line, header) in reverse.get(&child).into_iter().flatten() {
                if witnesses.contains_key(parent) {
                    continue;
                }
                let display = parent
                    .strip_prefix(root)
                    .unwrap_or(parent)
                    .display()
                    .to_string();
                witnesses.insert(
                    parent.clone(),
                    (*line, format!("{display}:{line} -> {header} -> {message}")),
                );
                pending.push_back(parent.clone());
            }
        }
        for source in &roots {
            if let Some((line, message)) = witnesses.get(source) {
                violations.insert(LintViolation {
                    checker: "EspArduinoHeaderChecker".to_string(),
                    path: source.strip_prefix(root).unwrap_or(source).display().to_string(),
                    line: *line,
                    message: format!("Native ESP dependency reaches an Arduino header: {message}. Use native SDK headers or an explicitly application-owned adapter."),
                });
            }
        }
    }
    violations.into_iter().collect()
}

#[cfg(test)]
mod esp_compile_command_tests {
    use super::*;

    #[test]
    fn esp_compile_database_profiles_include_shell_command_sdk_paths() {
        let unique = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap()
            .as_nanos();
        let root = std::env::temp_dir().join(format!(
            "fastled_esp_compile_commands_{}_{}",
            std::process::id(),
            unique
        ));
        let sdk = root.join("vendor sdk");
        fs::create_dir_all(&sdk).unwrap();
        for command_only in [false, true] {
            let mut entry = serde_json::json!({"directory": root, "file": "native.cpp"});
            if command_only {
                // Spaces in SDK directories must remain a single include flag.
                entry["command"] = serde_json::json!("c++ -DESP32 '-Ivendor sdk' -c native.cpp");
            } else {
                entry["arguments"] =
                    serde_json::json!(["c++", "-DESP32", "-Ivendor sdk", "-c", "native.cpp"]);
            }
            fs::write(
                root.join("compile_commands.json"),
                serde_json::to_string(&vec![entry]).unwrap(),
            )
            .unwrap();
            let includes_sdk = esp_dependency_include_dirs(&root)
                .iter()
                .flatten()
                .any(|include| include.path == sdk);
            assert!(
                includes_sdk,
                "SDK profile missing for command_only={command_only}"
            );
        }
        fs::remove_dir_all(root).unwrap();
    }
}
