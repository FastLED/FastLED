import json
from dataclasses import dataclass

import httpx

API_URL: str = "https://gitlab.com/api/v4/projects/4207231/releases"


def _fetch_all_releases() -> list[dict]:
    """
    Fetch the list of Graphviz releases from GitLab's API.

    Returns:
        A list of release dictionaries.
    """
    response = httpx.get(API_URL, timeout=10)
    response.raise_for_status()
    out = response.json()
    return out


@dataclass
class Releases:
    windows: list[str]
    cygwin: list[str]
    fedora: list[str]
    darwin: list[str]
    ubuntu: list[str]
    msys2: list[str]

    def __str__(self):
        return json.dumps(self.__dict__, indent=2)


def classify_assets(releases: list[dict]) -> Releases:
    """
    Extracts and classifies asset download URLs by operating system.

    Args:
        releases: list of release dictionaries from GitLab.

    Returns:
        A dictionary mapping OS names to lists of download URLs.
    """
    out: dict[str, list[str]] = {
        "windows": [],
        "cygwin": [],
        "fedora": [],
        "darwin": [],
        "ubuntu": [],
        "msys2": [],
    }

    for release in releases:
        assets = release.get("assets", {}).get("links", [])
        for asset in assets:
            name: str = asset.get("name", "")
            url: str = asset.get("url", "")
            if "zip" not in url and "xz" not in url:
                continue
            if "windows" in name.lower():
                out["windows"].append(url)
            elif "fedora" in name.lower():
                out["fedora"].append(url)
            elif "darwin" in name.lower():
                out["darwin"].append(url)
            elif "ubuntu" in name.lower():
                out["ubuntu"].append(url)
            elif "cygwin" in name.lower():
                out["cygwin"].append(url)
            elif "msys2" in name.lower():
                out["msys2"].append(url)

    return Releases(
        windows=out["windows"],
        cygwin=out["cygwin"],
        fedora=out["fedora"],
        darwin=out["darwin"],
        ubuntu=out["ubuntu"],
        msys2=out["msys2"],
    )


def limit_to_newest(assets: Releases, limit: int = 2) -> Releases:
    """
    Limit the number of releases to fetch.

    Args:
        releases: The list of releases to limit.
        limit: The number of releases to fetch.

    Returns:
        A list of release dictionaries.
    """
    return Releases(
        windows=assets.windows[:limit],
        cygwin=assets.cygwin[:limit],
        fedora=assets.fedora[:limit],
        darwin=assets.darwin[:limit],
        ubuntu=assets.ubuntu[:limit],
        msys2=assets.msys2[:limit],
    )


def fetch_releases(limit: int = 3) -> Releases:
    """
    Fetch the latest Graphviz releases from GitLab.

    Args:
        limit: The number of releases to fetch.

    Returns:
        A list of release dictionaries.
    """
    data: list[dict] = _fetch_all_releases()
    assets = classify_assets(data)
    assets = limit_to_newest(assets, limit)
    return assets


def main() -> Releases:
    """
    Fetch and classify Graphviz release binaries for Windows, Linux, and macOS.

    Returns:
        A dict with OS keys and lists of binary download URLs.
    """
    return fetch_releases()


if __name__ == "__main__":
    assets: Releases = main()
    print(assets)
