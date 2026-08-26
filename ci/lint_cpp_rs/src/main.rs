use std::process::ExitCode;

fn main() -> ExitCode {
    let args: Vec<String> = std::env::args().skip(1).collect();
    if args == ["--update-file-legal"] || args == ["--rescan-file-legal-history"] {
        let force_history_rescan = args == ["--rescan-file-legal-history"];
        return match std::env::current_dir()
            .map_err(|error| error.into())
            .and_then(|root| fastled_lint::update_file_legal_headers(&root, force_history_rescan))
        {
            Ok(changed) => {
                println!("Updated {changed} FASTLED-FILE-LEGAL block(s).");
                ExitCode::SUCCESS
            }
            Err(err) => {
                eprintln!("{err}");
                ExitCode::from(2)
            }
        };
    }
    match fastled_lint::run_cli(args) {
        Ok(code) => ExitCode::from(code),
        Err(err) => {
            eprintln!("{err}");
            ExitCode::from(2)
        }
    }
}
