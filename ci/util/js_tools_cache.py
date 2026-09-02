"""Repository-local cache paths for the JavaScript lint toolchain."""

from pathlib import Path


def repository_tools_dir(start: Path) -> Path:
    """Return one JS-tools cache shared by all worktrees of this repository."""
    start = start.resolve()
    for repository in (start, *start.parents):
        dot_git = repository / ".git"
        if dot_git.is_dir():
            common_git_dir = dot_git
        elif dot_git.is_file():
            marker = dot_git.read_text(encoding="utf-8").strip()
            prefix = "gitdir: "
            if not marker.startswith(prefix):
                continue
            git_dir = Path(marker.removeprefix(prefix))
            if not git_dir.is_absolute():
                git_dir = repository / git_dir
            git_dir = git_dir.resolve()
            commondir_file = git_dir / "commondir"
            if commondir_file.is_file():
                common_git_dir = (
                    git_dir / commondir_file.read_text(encoding="utf-8").strip()
                ).resolve()
            else:
                common_git_dir = git_dir
        else:
            continue

        return common_git_dir.parent / ".cache" / "js-tools"

    return start / ".cache" / "js-tools"
