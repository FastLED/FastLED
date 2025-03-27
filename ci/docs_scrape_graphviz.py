import json

import httpx

API_URL: str = "https://gitlab.com/api/v4/projects/4207231/releases"


def fetch_releases() -> list[dict]:
    """
    Fetch the list of Graphviz releases from GitLab's API.

    Returns:
        A list of release dictionaries.
    """
    response = httpx.get(API_URL, timeout=10)
    response.raise_for_status()
    return response.json()


def classify_assets(releases: list[dict]) -> dict[str, list[str]]:
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
            print(name)
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

    return out


def main() -> dict[str, list[str]]:
    """
    Fetch and classify Graphviz release binaries for Windows, Linux, and macOS.

    Returns:
        A dict with OS keys and lists of binary download URLs.
    """
    releases = fetch_releases()
    return classify_assets(releases)


if __name__ == "__main__":
    dict_info = main()

    for os_name, urls in dict_info.items():
        value_json = json.dumps(urls, indent=2)
        print(f"{os_name}:\n{value_json}\n")
