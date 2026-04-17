"""Sync GitHub issues and pull requests to a GitHub Projects v2 board.

Reads configuration from environment variables set by the
project_automation.yml workflow. Uses the GitHub GraphQL API to:
  - look up a project by owner + number
  - add the issue or PR to the project
  - set a Status single-select field
  - optionally set a Date field

Requires a GitHub App token with project read/write scope.
"""

from __future__ import annotations

import json
import os
import subprocess
import sys
from dataclasses import dataclass
from datetime import datetime, timezone
from typing import Any

from typeguard import typechecked


VALID_OWNER_TYPES = {"organization", "user"}


@typechecked
@dataclass
class FieldOption:
    """Resolved field and option IDs for a single-select project field."""

    field_id: str
    option_id: str


@typechecked
@dataclass
class Config:
    """Configuration parsed from environment variables."""

    project_owner: str
    project_owner_type: str  # "organization" or "user"
    project_number: int
    status_field: str
    status_todo: str
    status_done: str
    status_draft: str  # empty string means unused
    date_field: str  # empty string means unused
    event_name: str
    event_action: str
    item_node_id: str
    is_draft: bool

    @classmethod
    def from_env(cls: type[Config]) -> Config:
        """Parse configuration from environment variables."""
        project_number_raw = os.environ.get("PROJECT_NUMBER", "")
        if not project_number_raw:
            raise SystemExit("PROJECT_NUMBER is required")

        item_node_id = os.environ.get("ITEM_NODE_ID", "")
        if not item_node_id:
            raise SystemExit("ITEM_NODE_ID is required (no issue or PR node ID)")

        return cls(
            project_owner=os.environ.get("PROJECT_OWNER", ""),
            project_owner_type=os.environ.get("PROJECT_OWNER_TYPE", "organization"),
            project_number=int(project_number_raw),
            status_field=os.environ.get("STATUS_FIELD", "Status"),
            status_todo=os.environ.get("STATUS_TODO", "Todo"),
            status_done=os.environ.get("STATUS_DONE", "Done"),
            status_draft=os.environ.get("STATUS_DRAFT", ""),
            date_field=os.environ.get("DATE_FIELD", ""),
            event_name=os.environ.get("EVENT_NAME", ""),
            event_action=os.environ.get("EVENT_ACTION", ""),
            item_node_id=item_node_id,
            is_draft=os.environ.get("IS_DRAFT", "false").lower() == "true",
        )


def graphql(query: str, variables: dict[str, Any]) -> dict[str, Any]:
    """Execute a GitHub GraphQL query via the gh CLI."""
    cmd = ["gh", "api", "graphql", "-f", f"query={query}"]
    for key, value in variables.items():
        cmd.extend(["-F", f"{key}={value}"])

    result = subprocess.run(cmd, capture_output=True, text=True, check=False)
    if result.returncode != 0:
        print(f"GraphQL error (stderr): {result.stderr}", file=sys.stderr)
        raise SystemExit(f"GraphQL request failed: {result.returncode}")

    data: dict[str, Any] = json.loads(result.stdout)
    if "errors" in data:
        print(
            f"GraphQL errors: {json.dumps(data['errors'], indent=2)}", file=sys.stderr
        )
        raise SystemExit("GraphQL returned errors")

    return data


def find_project(owner: str, owner_type: str, number: int) -> str:
    """Look up a project by owner and number, return the project node ID."""
    if owner_type not in VALID_OWNER_TYPES:
        raise SystemExit(
            f"PROJECT_OWNER_TYPE must be 'organization' or 'user', got '{owner_type}'"
        )

    if owner_type == "organization":
        query = """
        query($owner: String!, $number: Int!) {
          organization(login: $owner) {
            projectV2(number: $number) {
              id
            }
          }
        }
        """
        data = graphql(query, {"owner": owner, "number": number})
        project = data["data"]["organization"]["projectV2"]
        if not project:
            raise SystemExit(f"Project #{number} not found for organization '{owner}'")
        return project["id"]
    else:
        query = """
        query($owner: String!, $number: Int!) {
          user(login: $owner) {
            projectV2(number: $number) {
              id
            }
          }
        }
        """
        data = graphql(query, {"owner": owner, "number": number})
        project = data["data"]["user"]["projectV2"]
        if not project:
            raise SystemExit(f"Project #{number} not found for user '{owner}'")
        return project["id"]


def add_item_to_project(project_id: str, content_id: str) -> str:
    """Add an issue or PR to the project. Returns the project item ID."""
    query = """
    mutation($projectId: ID!, $contentId: ID!) {
      addProjectV2ItemById(input: {projectId: $projectId, contentId: $contentId}) {
        item {
          id
        }
      }
    }
    """
    data = graphql(query, {"projectId": project_id, "contentId": content_id})
    return data["data"]["addProjectV2ItemById"]["item"]["id"]


def get_field_and_option(
    project_id: str, field_name: str, option_name: str
) -> FieldOption:
    """Find a single-select field and a specific option by name."""
    query = """
    query($projectId: ID!) {
      node(id: $projectId) {
        ... on ProjectV2 {
          fields(first: 50) {
            nodes {
              ... on ProjectV2SingleSelectField {
                id
                name
                options {
                  id
                  name
                }
              }
            }
          }
        }
      }
    }
    """
    data = graphql(query, {"projectId": project_id})
    fields: list[dict[str, Any]] = data["data"]["node"]["fields"]["nodes"]

    for field in fields:
        if field.get("name") == field_name:
            for option in field.get("options", []):
                if option["name"] == option_name:
                    return FieldOption(field_id=field["id"], option_id=option["id"])
            available: list[str] = []
            for opt in field.get("options", []):
                available.append(opt["name"])
            raise SystemExit(
                f"Option '{option_name}' not found in field '{field_name}'. "
                f"Available: {available}"
            )

    available_fields: list[str] = []
    for f in fields:
        name = f.get("name")
        if name:
            available_fields.append(name)
    raise SystemExit(f"Field '{field_name}' not found. Available: {available_fields}")


def get_date_field(project_id: str, field_name: str) -> str:
    """Find a date field by name. Returns field_id."""
    query = """
    query($projectId: ID!) {
      node(id: $projectId) {
        ... on ProjectV2 {
          fields(first: 50) {
            nodes {
              ... on ProjectV2Field {
                id
                name
                dataType
              }
            }
          }
        }
      }
    }
    """
    data = graphql(query, {"projectId": project_id})
    fields: list[dict[str, Any]] = data["data"]["node"]["fields"]["nodes"]

    for field in fields:
        if field.get("name") == field_name and field.get("dataType") == "DATE":
            return field["id"]

    raise SystemExit(f"Date field '{field_name}' not found in project")


def update_single_select(
    project_id: str, item_id: str, field_id: str, option_id: str
) -> None:
    """Update a single-select field on a project item."""
    query = """
    mutation($projectId: ID!, $itemId: ID!, $fieldId: ID!, $optionId: String!) {
      updateProjectV2ItemFieldValue(input: {
        projectId: $projectId
        itemId: $itemId
        fieldId: $fieldId
        value: {singleSelectOptionId: $optionId}
      }) {
        projectV2Item { id }
      }
    }
    """
    graphql(
        query,
        {
            "projectId": project_id,
            "itemId": item_id,
            "fieldId": field_id,
            "optionId": option_id,
        },
    )


def update_date(project_id: str, item_id: str, field_id: str, date_value: str) -> None:
    """Update a date field on a project item."""
    query = """
    mutation($projectId: ID!, $itemId: ID!, $fieldId: ID!, $dateValue: Date!) {
      updateProjectV2ItemFieldValue(input: {
        projectId: $projectId
        itemId: $itemId
        fieldId: $fieldId
        value: {date: $dateValue}
      }) {
        projectV2Item { id }
      }
    }
    """
    graphql(
        query,
        {
            "projectId": project_id,
            "itemId": item_id,
            "fieldId": field_id,
            "dateValue": date_value,
        },
    )


def determine_status(config: Config) -> str:
    """Determine what status to set based on the event."""
    if config.event_action == "closed":
        return config.status_done

    if (
        config.event_name == "pull_request_target"
        and config.is_draft
        and config.status_draft
    ):
        return config.status_draft

    return config.status_todo


def main() -> None:
    """Entry point: parse config, sync item to project, set status and date."""
    config = Config.from_env()

    if not config.project_owner:
        raise SystemExit("PROJECT_OWNER is required")

    print(f"Looking up project: {config.project_owner}#{config.project_number}")
    project_id = find_project(
        config.project_owner, config.project_owner_type, config.project_number
    )
    print(f"Project ID: {project_id}")

    print(f"Adding item {config.item_node_id} to project")
    item_id = add_item_to_project(project_id, config.item_node_id)
    print(f"Project item ID: {item_id}")

    status_name = determine_status(config)
    print(f"Setting status to: {status_name}")
    resolved = get_field_and_option(project_id, config.status_field, status_name)
    update_single_select(project_id, item_id, resolved.field_id, resolved.option_id)
    print("Status updated")

    if config.date_field and config.event_action in ("opened", "ready_for_review"):
        today = datetime.now(timezone.utc).strftime("%Y-%m-%d")
        print(f"Setting {config.date_field} to {today}")
        date_field_id = get_date_field(project_id, config.date_field)
        update_date(project_id, item_id, date_field_id, today)
        print("Date updated")

    print("Done")


if __name__ == "__main__":
    main()
