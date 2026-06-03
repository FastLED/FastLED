use std::process::ExitCode;

fn main() -> ExitCode {
    match fastled_lint::run_cli(std::env::args().skip(1)) {
        Ok(code) => ExitCode::from(code),
        Err(err) => {
            eprintln!("{err}");
            ExitCode::from(2)
        }
    }
}
