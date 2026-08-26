const FILE_LEGAL_BEGIN: &str = "// FASTLED-FILE-LEGAL-BEGIN";
const FILE_LEGAL_END: &str = "// FASTLED-FILE-LEGAL-END";
const FILE_LEGAL_SCHEMA: &str = "fastled-file-legal/v1";
const FILE_LEGAL_POLICY_YAML: &str = include_str!("../../file_legal_policy.yaml");

#[derive(Debug, Deserialize)]
#[serde(deny_unknown_fields)]
struct FileLegalPolicy {
    schema: String,
    enabled: bool,
    attribution_mode: String,
    recheck_after_days: i64,
    license: FileLegalLicense,
    ignored_identities: Vec<String>,
    owners: Vec<FileLegalOwnerPolicy>,
}

#[derive(Debug, Deserialize)]
#[serde(deny_unknown_fields)]
struct FileLegalOwnerPolicy {
    name: String,
    #[serde(default)]
    aliases: Vec<String>,
    #[serde(default)]
    always: bool,
}

#[derive(Debug, Deserialize, PartialEq, Eq)]
#[serde(deny_unknown_fields)]
struct FileLegalLicense {
    spdx_identifier: String,
    notice: String,
}

#[derive(Debug, Deserialize)]
#[serde(deny_unknown_fields)]
struct FileLegalDocument {
    schema: String,
    copyright: FileLegalCopyright,
    license: FileLegalLicense,
    audit: FileLegalAudit,
}

#[derive(Debug, Deserialize)]
#[serde(deny_unknown_fields)]
struct FileLegalCopyright {
    holders: Vec<FileLegalHolder>,
}

#[derive(Debug, Deserialize)]
#[serde(deny_unknown_fields)]
struct FileLegalHolder {
    name: String,
    years: FileLegalYears,
}

#[derive(Debug, Deserialize)]
#[serde(deny_unknown_fields)]
struct FileLegalYears {
    first: i64,
    last: i64,
}

#[derive(Debug, Deserialize)]
#[serde(deny_unknown_fields)]
struct FileLegalAudit {
    created: String,
    last_updated: String,
    recheck_after_days: i64,
    #[serde(default)]
    body_blake3: String,
}

struct FileLegalChecker {
    today_days: i64,
}

impl FileLegalChecker {
    fn today() -> Self {
        let epoch_days = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap_or_default()
            .as_secs() as i64
            / 86_400;
        Self { today_days: epoch_days }
    }

    #[cfg(test)]
    fn for_date(date: &str) -> Result<Self, String> {
        Ok(Self {
            today_days: parse_iso_date(date)?,
        })
    }
}

impl FileContentChecker for FileLegalChecker {
    fn name(&self) -> &'static str {
        "FileLegalChecker"
    }

    fn should_process_file(&self, file_path: &str, project_root: &Path) -> bool {
        let path = normalize_path(file_path);
        file_legal_policy().is_ok_and(|policy| policy.enabled)
            && is_under_project_subpath(&path, project_root, "src")
            && !is_under_project_subpath(&path, project_root, "src/third_party")
            && ends_with_any(&path, &[".h", ".hpp", ".cpp", ".c", ".S", ".s"])
    }

    fn check_file_content(&self, file_content: &FileContent) -> Vec<(usize, String)> {
        match check_file_legal_document(&file_content.content, self.today_days) {
            Ok(()) => Vec::new(),
            Err((line, message)) => vec![(line, message)],
        }
    }
}

fn file_legal_policy() -> Result<&'static FileLegalPolicy, String> {
    static POLICY: OnceLock<Result<FileLegalPolicy, String>> = OnceLock::new();
    POLICY
        .get_or_init(|| {
            let policy: FileLegalPolicy = serde_yaml::from_str(FILE_LEGAL_POLICY_YAML)
                .map_err(|error| format!("invalid file legal policy: {error}"))?;
            if policy.schema != "fastled-file-legal-policy/v1" {
                return Err(format!("unsupported file legal policy schema: {}", policy.schema));
            }
            if policy.attribution_mode != "initial_history_snapshot" {
                return Err(format!(
                    "unsupported file legal attribution mode: {}",
                    policy.attribution_mode
                ));
            }
            if policy.recheck_after_days <= 0 {
                return Err("file legal recheck_after_days must be positive".to_string());
            }
            if !policy.owners.iter().any(|owner| owner.name == "Zach Vorhies" && owner.always) {
                return Err("file legal policy must always include Zach Vorhies".to_string());
            }
            let _alias_count: usize = policy.owners.iter().map(|owner| owner.aliases.len()).sum();
            Ok(policy)
        })
        .as_ref()
        .map_err(Clone::clone)
}

fn check_file_legal_document(content: &str, today_days: i64) -> Result<(), (usize, String)> {
    if content
        .strip_prefix('\u{feff}')
        .unwrap_or(content)
        .contains('\u{feff}')
    {
        return Err((
            1,
            "UTF-8 BOM is only valid at the start of the file".to_string(),
        ));
    }
    let begin_lines: Vec<usize> = content
        .lines()
        .enumerate()
        .filter_map(|(index, line)| (line.trim_start_matches('\u{feff}') == FILE_LEGAL_BEGIN).then_some(index))
        .collect();
    let end_lines: Vec<usize> = content
        .lines()
        .enumerate()
        .filter_map(|(index, line)| (line == FILE_LEGAL_END).then_some(index))
        .collect();

    if begin_lines.is_empty() && end_lines.is_empty() {
        return Err((1, "missing FASTLED-FILE-LEGAL block; run the Rust legal-header updater".to_string()));
    }
    if begin_lines.len() != 1 || end_lines.len() != 1 {
        return Err((1, "expected exactly one FASTLED-FILE-LEGAL block".to_string()));
    }
    let begin = begin_lines[0];
    let end = end_lines[0];
    if end <= begin {
        return Err((begin + 1, "FASTLED-FILE-LEGAL markers are out of order".to_string()));
    }
    if begin != 0 {
        return Err((begin + 1, "FASTLED-FILE-LEGAL block must be the first file content".to_string()));
    }

    let lines: Vec<&str> = content.lines().collect();
    let mut yaml = String::new();
    for (offset, line) in lines[begin + 1..end].iter().enumerate() {
        let Some(rest) = line.strip_prefix("//") else {
            return Err((begin + offset + 2, "FASTLED-FILE-LEGAL content must be line-comment YAML".to_string()));
        };
        let rest = rest.strip_prefix(' ').unwrap_or(rest);
        yaml.push_str(rest);
        yaml.push('\n');
    }

    let document: FileLegalDocument = serde_yaml::from_str(&yaml)
        .map_err(|error| (begin + 2, format!("invalid FASTLED-FILE-LEGAL YAML: {error}")))?;
    let policy = file_legal_policy().map_err(|error| (begin + 2, error))?;
    if document.schema != FILE_LEGAL_SCHEMA {
        return Err((begin + 2, format!("expected legal schema {FILE_LEGAL_SCHEMA}")));
    }
    let actual_holders: Vec<&str> = document
        .copyright
        .holders
        .iter()
        .map(|holder| holder.name.as_str())
        .collect();
    let mut unique_holders = HashSet::new();
    if actual_holders.iter().any(|name| !unique_holders.insert(*name)) {
        return Err((begin + 3, "copyright holders must be unique".to_string()));
    }
    if actual_holders.iter().any(|name| name.contains('@')) {
        return Err((
            begin + 3,
            "copyright holder names must not contain email addresses".to_string(),
        ));
    }
    if let Some(ignored) = actual_holders.iter().find(|name| {
        policy
            .ignored_identities
            .iter()
            .any(|candidate| candidate.eq_ignore_ascii_case(name))
            || name.ends_with("[bot]")
    }) {
        return Err((
            begin + 3,
            format!("ignored automation identity must not be a copyright holder: {ignored}"),
        ));
    }
    if actual_holders.last().copied() != Some("Zach Vorhies") {
        return Err((begin + 3, "copyright holders must append Zach Vorhies last".to_string()));
    }
    for owner in &policy.owners {
        if owner.always && !actual_holders.contains(&owner.name.as_str()) {
            return Err((begin + 3, format!("copyright holders must include {}", owner.name)));
        }
    }
    for holder in &document.copyright.holders {
        if holder.years.first > holder.years.last {
            return Err((begin + 3, format!("invalid copyright year range for {}", holder.name)));
        }
    }
    if document.license != policy.license {
        return Err((begin + 3, format!("license must match current policy ({})", policy.license.spdx_identifier)));
    }
    if document.audit.recheck_after_days != policy.recheck_after_days {
        return Err((begin + 3, format!("recheck_after_days must be {}", policy.recheck_after_days)));
    }
    let created = parse_iso_date(&document.audit.created).map_err(|error| (begin + 3, error))?;
    let last_updated = parse_iso_date(&document.audit.last_updated).map_err(|error| (begin + 3, error))?;
    if created > last_updated {
        return Err((begin + 3, "audit.created must not be after audit.last_updated".to_string()));
    }
    if last_updated > today_days {
        return Err((begin + 3, "audit.last_updated must not be in the future".to_string()));
    }
    let current_body_hash = file_legal_body_hash(content)
        .map_err(|message| (begin + 1, message))?;
    if document.audit.body_blake3.len() != 64
        || !document.audit.body_blake3.bytes().all(|byte| byte.is_ascii_hexdigit())
    {
        return Err((begin + 3, "audit.body_blake3 must be a 64-character hex digest".to_string()));
    }
    if current_body_hash != document.audit.body_blake3
        && today_days - last_updated > policy.recheck_after_days
    {
        return Err((begin + 3, format!("legal notice audit expired after {} days; run the Rust legal-header updater", policy.recheck_after_days)));
    }
    Ok(())
}

fn file_legal_body(content: &str) -> Result<&str, String> {
    let content = content.strip_prefix('\u{feff}').unwrap_or(content);
    let begins: Vec<usize> = content.match_indices(FILE_LEGAL_BEGIN).map(|(index, _)| index).collect();
    let ends: Vec<usize> = content.match_indices(FILE_LEGAL_END).map(|(index, _)| index).collect();
    if begins.is_empty() && ends.is_empty() {
        return Ok(content);
    }
    if begins.len() != 1 || ends.len() != 1 || begins[0] != 0 || ends[0] <= begins[0] {
        return Err("cannot hash malformed FASTLED-FILE-LEGAL markers".to_string());
    }
    let end = ends[0] + FILE_LEGAL_END.len();
    let body_start = if content[end..].starts_with("\r\n") {
        end + 2
    } else if content[end..].starts_with('\n') {
        end + 1
    } else {
        end
    };
    Ok(&content[body_start..])
}

fn file_legal_body_hash(content: &str) -> Result<String, String> {
    Ok(blake3::hash(file_legal_body(content)?.as_bytes()).to_hex().to_string())
}

fn parse_iso_date(value: &str) -> Result<i64, String> {
    let bytes = value.as_bytes();
    if bytes.len() != 10 || bytes[4] != b'-' || bytes[7] != b'-' {
        return Err(format!("invalid ISO date {value:?}; expected YYYY-MM-DD"));
    }
    let year = value[0..4].parse::<i64>().map_err(|_| format!("invalid ISO date {value:?}"))?;
    let month = value[5..7].parse::<i64>().map_err(|_| format!("invalid ISO date {value:?}"))?;
    let day = value[8..10].parse::<i64>().map_err(|_| format!("invalid ISO date {value:?}"))?;
    if !(1..=12).contains(&month) || day < 1 || day > days_in_month(year, month) {
        return Err(format!("invalid ISO date {value:?}"));
    }
    Ok(days_from_civil(year, month, day))
}

fn days_in_month(year: i64, month: i64) -> i64 {
    match month {
        2 if year % 4 == 0 && (year % 100 != 0 || year % 400 == 0) => 29,
        2 => 28,
        4 | 6 | 9 | 11 => 30,
        _ => 31,
    }
}

fn days_from_civil(mut year: i64, month: i64, day: i64) -> i64 {
    year -= i64::from(month <= 2);
    let era = if year >= 0 { year } else { year - 399 } / 400;
    let year_of_era = year - era * 400;
    let adjusted_month = month + if month > 2 { -3 } else { 9 };
    let day_of_year = (153 * adjusted_month + 2) / 5 + day - 1;
    let day_of_era = year_of_era * 365 + year_of_era / 4 - year_of_era / 100 + day_of_year;
    era * 146_097 + day_of_era - 719_468
}

#[derive(Clone, Debug)]
struct FileLegalHistory {
    created: String,
    authors: Vec<FileLegalHistoryAuthor>,
}

#[derive(Clone, Debug)]
struct FileLegalHistoryAuthor {
    name: String,
    first_year: i64,
    last_year: i64,
}

impl FileLegalHistory {
    fn touch(&mut self, raw_name: &str, date: &str, policy: &FileLegalPolicy) {
        let name = canonical_file_legal_name(raw_name, policy);
        if policy
            .ignored_identities
            .iter()
            .any(|ignored| ignored.eq_ignore_ascii_case(&name))
            || name.ends_with("[bot]")
        {
            return;
        }
        let year = date
            .get(0..4)
            .and_then(|value| value.parse().ok())
            .unwrap_or(0);
        if let Some(author) = self.authors.iter_mut().find(|author| author.name == name) {
            author.first_year = author.first_year.min(year);
            author.last_year = author.last_year.max(year);
        } else {
            self.authors.push(FileLegalHistoryAuthor {
                name,
                first_year: year,
                last_year: year,
            });
        }
    }
}

fn canonical_file_legal_name(raw_name: &str, policy: &FileLegalPolicy) -> String {
    let raw_name = raw_name
        .trim()
        .split_once('@')
        .map_or(raw_name.trim(), |(local_part, _)| local_part);
    for owner in &policy.owners {
        if owner.name.eq_ignore_ascii_case(raw_name)
            || owner
                .aliases
                .iter()
                .any(|alias| alias.eq_ignore_ascii_case(raw_name))
        {
            return owner.name.clone();
        }
    }
    raw_name.to_string()
}

fn existing_file_legal_history(content: &str) -> Result<Option<FileLegalHistory>, String> {
    let content = content.strip_prefix('\u{feff}').unwrap_or(content);
    let begin_count = content.matches(FILE_LEGAL_BEGIN).count();
    let end_count = content.matches(FILE_LEGAL_END).count();
    if begin_count == 0 && end_count == 0 {
        return Ok(None);
    }
    if begin_count != 1 || end_count != 1 || !content.starts_with(FILE_LEGAL_BEGIN) {
        return Err("refusing to update malformed FASTLED-FILE-LEGAL markers".to_string());
    }
    let end_start = content
        .find(FILE_LEGAL_END)
        .ok_or_else(|| "missing FASTLED-FILE-LEGAL end marker".to_string())?;
    if end_start <= FILE_LEGAL_BEGIN.len() {
        return Err("FASTLED-FILE-LEGAL markers are out of order".to_string());
    }
    let yaml_region = &content[FILE_LEGAL_BEGIN.len()..end_start];
    let mut yaml = String::new();
    for line in yaml_region.lines().skip(1) {
        let rest = line
            .strip_prefix("//")
            .ok_or_else(|| "FASTLED-FILE-LEGAL content must be line-comment YAML".to_string())?;
        yaml.push_str(rest.strip_prefix(' ').unwrap_or(rest));
        yaml.push('\n');
    }
    let document: FileLegalDocument = serde_yaml::from_str(&yaml)
        .map_err(|error| format!("invalid FASTLED-FILE-LEGAL YAML: {error}"))?;
    Ok(Some(FileLegalHistory {
        created: document.audit.created,
        authors: document
            .copyright
            .holders
            .into_iter()
            .map(|holder| FileLegalHistoryAuthor {
                name: holder.name,
                first_year: holder.years.first,
                last_year: holder.years.last,
            })
            .collect(),
    }))
}

fn collect_file_legal_history(
    project_root: &Path,
) -> Result<HashMap<String, FileLegalHistory>, DynError> {
    let inventory = std::process::Command::new("git")
        .current_dir(project_root)
        .env("GIT_OPTIONAL_LOCKS", "0")
        .args(["--no-optional-locks", "ls-files", "-z", "--", "src"])
        .output()?;
    if !inventory.status.success() {
        return Err(format!(
            "git file inventory failed: {}",
            String::from_utf8_lossy(&inventory.stderr)
        )
        .into());
    }
    let files: Vec<String> = inventory
        .stdout
        .split(|byte| *byte == 0)
        .filter(|path| !path.is_empty())
        .map(|path| normalize_path(&String::from_utf8_lossy(path)))
        .filter(|path| {
            !path.starts_with("src/third_party/")
                && ends_with_any(path, &[".h", ".hpp", ".cpp", ".c", ".S", ".s"])
        })
        .collect();
    collect_file_legal_history_for_paths(project_root, &files)
}

fn collect_file_legal_history_for_paths(
    project_root: &Path,
    files: &[String],
) -> Result<HashMap<String, FileLegalHistory>, DynError> {
    let policy = file_legal_policy().map_err(|error| -> DynError { error.into() })?;
    let histories: Result<Vec<(String, FileLegalHistory)>, String> = files
        .par_iter()
        .map(|path| {
            let output = std::process::Command::new("git")
                .current_dir(project_root)
                .env("GIT_OPTIONAL_LOCKS", "0")
                .args([
                    "--no-optional-locks",
                    "log",
                    "--follow",
                    "--date=short",
                    "--format=%ad%x00%aN%x00",
                    "--",
                    path,
                ])
                .output()
                .map_err(|error| format!("git history scan failed for {path}: {error}"))?;
            if !output.status.success() {
                return Err(format!(
                    "git history scan failed for {path}: {}",
                    String::from_utf8_lossy(&output.stderr)
                ));
            }
            let fields: Vec<&[u8]> = output
                .stdout
                .split(|byte| *byte == 0)
                .filter(|field| !field.is_empty())
                .collect();
            let created = fields
                .chunks_exact(2)
                .map(|pair| String::from_utf8_lossy(pair[0]).trim().to_string())
                .min()
                .unwrap_or_else(today_iso_date);
            let mut history = FileLegalHistory {
                created,
                authors: Vec::new(),
            };
            for pair in fields.chunks_exact(2) {
                let date = String::from_utf8_lossy(pair[0]);
                let author = String::from_utf8_lossy(pair[1]);
                history.touch(author.trim(), date.trim(), policy);
            }
            Ok((path.clone(), history))
        })
        .collect();
    Ok(histories?.into_iter().collect())
}

#[cfg(test)]
mod history_tests {
    use super::*;
    use std::process::Command;

    fn git(root: &Path, args: &[&str], author: Option<(&str, &str, &str)>) {
        let mut command = Command::new("git");
        command.current_dir(root).args(args);
        if let Some((name, email, date)) = author {
            command
                .env("GIT_AUTHOR_NAME", name)
                .env("GIT_AUTHOR_EMAIL", email)
                .env("GIT_AUTHOR_DATE", date)
                .env("GIT_COMMITTER_NAME", name)
                .env("GIT_COMMITTER_EMAIL", email)
                .env("GIT_COMMITTER_DATE", date);
        }
        let output = command.output().expect("git command must start");
        assert!(
            output.status.success(),
            "git {:?} failed: {}",
            args,
            String::from_utf8_lossy(&output.stderr)
        );
    }

    #[test]
    fn history_follows_a_file_moved_from_repository_root_into_src() {
        let root = std::env::temp_dir().join(format!(
            "fastled_file_legal_history_{}_{}",
            std::process::id(),
            std::time::SystemTime::now()
                .duration_since(std::time::UNIX_EPOCH)
                .unwrap()
                .as_nanos()
        ));
        if root.exists() {
            fs::remove_dir_all(&root).unwrap();
        }
        fs::create_dir_all(&root).unwrap();
        git(&root, &["init", "-q"], None);
        fs::write(root.join("FastLED.h"), "#pragma once\n").unwrap();
        git(&root, &["add", "FastLED.h"], None);
        git(
            &root,
            &["commit", "-q", "-m", "original header"],
            Some(("Daniel Garcia", "daniel.invalid", "2013-01-02T00:00:00Z")),
        );
        fs::create_dir_all(root.join("src")).unwrap();
        git(&root, &["mv", "FastLED.h", "src/FastLED.h"], None);
        git(
            &root,
            &["commit", "-q", "-m", "move sources"],
            Some(("Sam Guyer", "sam.invalid", "2020-06-13T00:00:00Z")),
        );

        let histories = collect_file_legal_history(&root).unwrap();
        let history = histories.get("src/FastLED.h").unwrap();
        assert_eq!(history.created, "2013-01-02");
        assert!(history.authors.iter().any(|author| {
            author.name == "Daniel Garcia"
                && author.first_year == 2013
                && author.last_year == 2013
        }));
        assert!(history.authors.iter().any(|author| author.name == "Sam Guyer"));

        fs::remove_dir_all(root).unwrap();
    }

    #[test]
    fn rendered_history_canonicalizes_email_shaped_legacy_names() {
        let policy = file_legal_policy().unwrap();
        let mut history = FileLegalHistory {
            created: "2013-01-02".to_string(),
            authors: Vec::new(),
        };
        let separator = char::from(64);
        history.touch(&format!("danielgarcia{separator}ignored"), "2013-01-02", policy);
        history.touch(&format!("kriegsman{separator}ignored"), "2014-01-02", policy);
        history.touch(&format!("unknown{separator}ignored"), "2015-01-02", policy);
        let rendered = render_file_legal_header(
            &history,
            "2026-08-25",
            policy,
            &blake3::hash(b"").to_hex().to_string(),
        );
        assert!(rendered.contains("name: \"Daniel Garcia\""));
        assert!(rendered.contains("name: \"Mark Kriegsman\""));
        assert!(rendered.contains("name: \"unknown\""));
        assert!(!rendered.contains('@'));
    }

    #[test]
    #[ignore = "one-off audit: requires the repository's complete Git history"]
    fn live_fastled_history_contains_known_pre_and_post_move_authors() {
        let project_root = Path::new(env!("CARGO_MANIFEST_DIR"))
            .parent()
            .and_then(Path::parent)
            .unwrap();
        let known_paths = [
            "src/FastLED.h",
            "src/chipsets.h",
            "src/colorutils.h",
            "src/fastspi.h",
            "src/pixeltypes.h",
            "src/platforms/esp/32/clockless_block_esp32.h",
        ];
        let histories = collect_file_legal_history_for_paths(
            project_root,
            &known_paths.iter().map(|path| (*path).to_string()).collect::<Vec<_>>(),
        )
        .unwrap();
        for path in [
            "src/FastLED.h",
            "src/chipsets.h",
            "src/colorutils.h",
            "src/fastspi.h",
        ] {
            let history = histories.get(path).unwrap_or_else(|| panic!("missing history for {path}"));
            assert!(
                history.authors.iter().any(|author| author.name == "Daniel Garcia"),
                "Daniel Garcia missing from {path}"
            );
        }
        for path in [
            "src/FastLED.h",
            "src/chipsets.h",
            "src/colorutils.h",
            "src/pixeltypes.h",
        ] {
            let history = histories.get(path).unwrap_or_else(|| panic!("missing history for {path}"));
            assert!(
                history.authors.iter().any(|author| author.name == "Mark Kriegsman"),
                "Mark Kriegsman missing from {path}"
            );
        }
        let esp32 = histories
            .get("src/platforms/esp/32/clockless_block_esp32.h")
            .expect("missing ESP32 clockless header history");
        assert!(esp32.authors.iter().any(|author| author.name == "Sam Guyer"));
    }

    #[test]
    #[ignore = "deep audit: launches one independent git --follow query per owned source file"]
    fn deep_live_history_matches_per_file_git_for_mark_and_sam() {
        let project_root = Path::new(env!("CARGO_MANIFEST_DIR"))
            .parent()
            .and_then(Path::parent)
            .unwrap();
        let policy = file_legal_policy().unwrap();
        let inventory = Command::new("git")
            .current_dir(project_root)
            .env("GIT_OPTIONAL_LOCKS", "0")
            .args(["--no-optional-locks", "ls-files", "-z", "--", "src"])
            .output()
            .unwrap();
        assert!(inventory.status.success());
        let files: Vec<String> = inventory
            .stdout
            .split(|byte| *byte == 0)
            .filter(|path| !path.is_empty())
            .map(|path| normalize_path(&String::from_utf8_lossy(path)))
            .filter(|path| {
                !path.starts_with("src/third_party/")
                    && ends_with_any(path, &[".h", ".hpp", ".cpp", ".c", ".S", ".s"])
            })
            .collect();

        let oracle: HashMap<String, HashSet<String>> = files
            .par_iter()
            .map(|path| {
                let output = Command::new("git")
                    .current_dir(project_root)
                    .env("GIT_OPTIONAL_LOCKS", "0")
                    .args([
                        "--no-optional-locks",
                        "log",
                        "--follow",
                        "--format=%aN%x00",
                        "--",
                        path,
                    ])
                    .output()
                    .unwrap();
                assert!(output.status.success(), "git --follow failed for {path}");
                let authors = output
                    .stdout
                    .split(|byte| *byte == 0)
                    .filter(|name| !name.is_empty())
                    .map(|name| {
                        canonical_file_legal_name(&String::from_utf8_lossy(name), policy)
                    })
                    .collect();
                (path.clone(), authors)
            })
            .collect();
        let batched = collect_file_legal_history(project_root).unwrap();

        for expected_author in ["Mark Kriegsman", "Sam Guyer"] {
            let expected: BTreeSet<&str> = oracle
                .iter()
                .filter(|(_, authors)| authors.contains(expected_author))
                .map(|(path, _)| path.as_str())
                .collect();
            let actual: BTreeSet<&str> = batched
                .iter()
                .filter(|(_, history)| {
                    history
                        .authors
                        .iter()
                        .any(|author| author.name == expected_author)
                })
                .map(|(path, _)| path.as_str())
                .filter(|path| files.iter().any(|candidate| candidate == path))
                .collect();
            let missing: Vec<_> = expected.difference(&actual).copied().collect();
            let extra: Vec<_> = actual.difference(&expected).copied().collect();
            eprintln!(
                "{expected_author}: oracle={}, batched={}, missing={}, extra={}",
                expected.len(),
                actual.len(),
                missing.len(),
                extra.len()
            );
            assert!(missing.is_empty(), "{expected_author} missing from {missing:?}");
            assert!(extra.is_empty(), "{expected_author} unexpectedly present in {extra:?}");
        }
    }
}

fn today_iso_date() -> String {
    let days = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap_or_default()
        .as_secs() as i64
        / 86_400;
    let (year, month, day) = civil_from_days(days);
    format!("{year:04}-{month:02}-{day:02}")
}

fn civil_from_days(days: i64) -> (i64, i64, i64) {
    let days = days + 719_468;
    let era = if days >= 0 {
        days
    } else {
        days - 146_096
    } / 146_097;
    let day_of_era = days - era * 146_097;
    let year_of_era = (day_of_era - day_of_era / 1_460 + day_of_era / 36_524
        - day_of_era / 146_096)
        / 365;
    let mut year = year_of_era + era * 400;
    let day_of_year =
        day_of_era - (365 * year_of_era + year_of_era / 4 - year_of_era / 100);
    let month_prime = (5 * day_of_year + 2) / 153;
    let day = day_of_year - (153 * month_prime + 2) / 5 + 1;
    let month = month_prime + if month_prime < 10 { 3 } else { -9 };
    year += i64::from(month <= 2);
    (year, month, day)
}

fn render_file_legal_header(
    history: &FileLegalHistory,
    today: &str,
    policy: &FileLegalPolicy,
    body_blake3: &str,
) -> String {
    let mut authors: Vec<FileLegalHistoryAuthor> = Vec::new();
    for author in &history.authors {
        let name = canonical_file_legal_name(&author.name, policy);
        if name.contains('@') {
            continue;
        }
        if policy
            .ignored_identities
            .iter()
            .any(|ignored| ignored.eq_ignore_ascii_case(&name))
            || name.ends_with("[bot]")
        {
            continue;
        }
        if let Some(existing) = authors.iter_mut().find(|existing| existing.name == name) {
            existing.first_year = existing.first_year.min(author.first_year);
            existing.last_year = existing.last_year.max(author.last_year);
        } else {
            authors.push(FileLegalHistoryAuthor {
                name,
                first_year: author.first_year,
                last_year: author.last_year,
            });
        }
    }
    authors.retain(|author| author.name != "Zach Vorhies");
    let today_year = today[0..4].parse::<i64>().unwrap_or(0);
    let zach = history
        .authors
        .iter()
        .find(|author| author.name == "Zach Vorhies")
        .cloned()
        .unwrap_or(FileLegalHistoryAuthor {
            name: "Zach Vorhies".to_string(),
            first_year: today_year,
            last_year: today_year,
        });
    authors.push(zach);

    let mut output = String::new();
    output.push_str(FILE_LEGAL_BEGIN);
    output.push_str("\n// schema: fastled-file-legal/v1\n//\n// copyright:\n//   holders:\n");
    for author in authors {
        let quoted_name = serde_json::to_string(&author.name)
            .unwrap_or_else(|_| "\"Unknown\"".to_string());
        output.push_str(&format!(
            "//     - {{name: {quoted_name}, years: {{first: {}, last: {}}}}}\n",
            author.first_year, author.last_year
        ));
    }
    output.push_str("//\n// license:\n");
    output.push_str(&format!(
        "//   spdx_identifier: {}\n",
        policy.license.spdx_identifier
    ));
    output.push_str("//   notice: |-\n");
    for line in policy.license.notice.lines() {
        output.push_str("//     ");
        output.push_str(line);
        output.push('\n');
    }
    output.push_str("//\n// audit:\n");
    output.push_str(&format!(
        "//   created: {}\n//   last_updated: {today}\n//   recheck_after_days: {}\n//   body_blake3: {body_blake3}\n",
        history.created, policy.recheck_after_days
    ));
    output.push_str(FILE_LEGAL_END);
    output.push('\n');
    output
}

pub fn update_file_legal_headers(
    project_root: &Path,
    force_history_rescan: bool,
) -> Result<usize, DynError> {
    let policy = file_legal_policy().map_err(|error| -> DynError { error.into() })?;
    if !policy.enabled {
        return Err("file legal compliance is disabled by policy".into());
    }
    let today = today_iso_date();
    let output = std::process::Command::new("git")
        .current_dir(project_root)
        .env("GIT_OPTIONAL_LOCKS", "0")
        .args(["--no-optional-locks", "ls-files", "-z", "--", "src"])
        .output()?;
    if !output.status.success() {
        return Err(format!(
            "git file inventory failed: {}",
            String::from_utf8_lossy(&output.stderr)
        )
        .into());
    }

    struct PendingFile {
        relative: String,
        path: PathBuf,
        original: String,
        existing_history: Option<FileLegalHistory>,
        body_blake3: String,
        had_bom: bool,
    }

    let mut pending = Vec::new();
    for raw_path in output
        .stdout
        .split(|byte| *byte == 0)
        .filter(|path| !path.is_empty())
    {
        let relative = normalize_path(&String::from_utf8_lossy(raw_path));
        if is_under_dir(&relative, "third_party")
            || !ends_with_any(&relative, &[".h", ".hpp", ".cpp", ".c", ".S", ".s"])
        {
            continue;
        }
        let path = project_root.join(&relative);
        let original = fs::read_to_string(&path)?;
        if !force_history_rescan
            && check_file_legal_document(&original, FileLegalChecker::today().today_days).is_ok()
        {
            continue;
        }
        let had_bom = original.contains('\u{feff}');
        let original = original.replace('\u{feff}', "");
        let existing_history = if force_history_rescan {
            None
        } else {
            existing_file_legal_history(&original)
                .map_err(|error| format!("{relative}: {error}"))?
        };
        let body_blake3 = file_legal_body_hash(&original)
            .map_err(|error| format!("{relative}: {error}"))?;
        pending.push(PendingFile {
            relative,
            path,
            original,
            existing_history,
            body_blake3,
            had_bom,
        });
    }

    let needs_history = pending.iter().any(|file| file.existing_history.is_none());
    let histories = if needs_history {
        Some(collect_file_legal_history(project_root)?)
    } else {
        None
    };
    let mut changed = 0;
    for pending_file in pending {
        let PendingFile {
            relative,
            path,
            original,
            existing_history,
            body_blake3,
            had_bom,
        } = pending_file;
        let history = existing_history
            .or_else(|| {
                histories
                    .as_ref()
                    .and_then(|items| items.get(&relative).cloned())
            })
            .unwrap_or(FileLegalHistory {
                created: today.clone(),
                authors: Vec::new(),
            });
        let header = render_file_legal_header(&history, &today, policy, &body_blake3);
        let crlf = original.contains("\r\n");
        let header = if crlf {
            header.replace('\n', "\r\n")
        } else {
            header
        };
        let bom = if had_bom { "\u{feff}" } else { "" };
        let original_body = original.as_str();
        let updated_body = if let (Some(begin), Some(end_start)) =
            (original_body.find(FILE_LEGAL_BEGIN), original_body.find(FILE_LEGAL_END))
        {
            let end = end_start + FILE_LEGAL_END.len();
            let suffix_start = if original_body[end..].starts_with("\r\n") {
                end + 2
            } else if original_body[end..].starts_with('\n') {
                end + 1
            } else {
                end
            };
            format!(
                "{}{}{}",
                &original_body[..begin],
                header,
                &original_body[suffix_start..]
            )
        } else {
            format!("{header}{original_body}")
        };
        let updated = format!("{bom}{updated_body}");
        if updated != original {
            fs::write(&path, updated)?;
            changed += 1;
        }
    }
    Ok(changed)
}
