# pyright: reportUnknownMemberType=false
import os
import unittest
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Dict, List, Optional, Union

import yaml

from ci.util.paths import PROJECT_ROOT


@dataclass
class WorkflowStep:
    name: Optional[str] = None
    uses: Optional[str] = None
    with_config: Optional[Dict[str, Any]] = None
    run: Optional[str] = None


@dataclass
class WorkflowJob:
    runs_on: Optional[Union[str, List[str]]] = None
    steps: Optional[List[WorkflowStep]] = None
    permissions: Optional[Dict[str, str]] = None

    def __post_init__(self) -> None:
        if self.steps is None:
            self.steps = []


@dataclass
class WorkflowTrigger:
    branches: Optional[List[str]] = None
    paths: Optional[List[str]] = None


@dataclass
class GitHubWorkflow:
    name: Optional[str] = None
    on_config: Optional[Dict[str, Any]] = None
    jobs: Optional[Dict[str, WorkflowJob]] = None
    permissions: Optional[Dict[str, str]] = None

    def __post_init__(self) -> None:
        if self.jobs is None:
            self.jobs = {}
        if self.on_config is None:
            self.on_config = {}


class TestGitHubActionsSecurityTest(unittest.TestCase):
    """
    Security tests for GitHub Actions workflows to prevent known vulnerabilities.

    This test ensures that workflows using pull_request_target have proper
    permissions restrictions to prevent the "pwn request" vulnerability:
    https://securitylab.github.com/resources/github-actions-preventing-pwn-requests/
    """

    def setUp(self):
        self.workflows_dir = PROJECT_ROOT / ".github" / "workflows"
        self.workflow_files = list(self.workflows_dir.glob("*.yml")) + list(
            self.workflows_dir.glob("*.yaml")
        )

    def _load_workflow(self, workflow_path: Path) -> GitHubWorkflow:
        """Load and parse a GitHub Actions workflow file into a dataclass."""
        try:
            with open(workflow_path, "r", encoding="utf-8") as f:
                raw_content: Any = yaml.safe_load(f)

            content: Dict[str, Any] = raw_content or {}

            # Handle the case where 'on' is parsed as boolean True instead of string 'on'
            # This happens because 'on' is a YAML boolean keyword
            if True in content and "on" not in content:
                on_data = content.pop(True)  # pyright: ignore[reportArgumentType]
                content["on"] = on_data

            # Parse jobs into dataclass
            jobs_dict: Dict[str, WorkflowJob] = {}
            raw_jobs: Dict[str, Any] = content.get("jobs", {})
            for job_name, job_data in raw_jobs.items():
                if not isinstance(job_data, dict):
                    continue

                # Parse steps
                steps: List[WorkflowStep] = []
                raw_steps: List[Any] = job_data.get("steps", [])  # pyright: ignore[reportUnknownVariableType]
                for step_data in raw_steps:  # pyright: ignore[reportUnknownVariableType]
                    if isinstance(step_data, dict):
                        step_dict: Dict[str, Any] = step_data  # pyright: ignore[reportUnknownVariableType]
                        step = WorkflowStep(
                            name=step_dict.get("name"),
                            uses=step_dict.get("uses"),
                            with_config=step_dict.get("with"),
                            run=step_dict.get("run"),
                        )
                        steps.append(step)

                job_dict: Dict[str, Any] = job_data  # pyright: ignore[reportUnknownVariableType]
                job = WorkflowJob(
                    runs_on=job_dict.get("runs-on"),
                    steps=steps,
                    permissions=job_dict.get("permissions"),
                )
                jobs_dict[job_name] = job

            return GitHubWorkflow(
                name=content.get("name"),
                on_config=content.get("on", {}),
                jobs=jobs_dict,
                permissions=content.get("permissions"),
            )
        except Exception as e:
            self.fail(f"Failed to parse workflow {workflow_path}: {e}")

    def _has_pull_request_target(self, workflow: GitHubWorkflow) -> bool:
        """Check if workflow uses pull_request_target trigger."""
        if isinstance(workflow.on_config, list):
            return "pull_request_target" in workflow.on_config
        elif isinstance(workflow.on_config, dict):
            return "pull_request_target" in workflow.on_config
        return False

    def _has_untrusted_code_checkout(self, workflow: GitHubWorkflow) -> bool:
        """Check if workflow checks out PR code that could be untrusted."""
        if workflow.jobs is None:
            return False

        for job in workflow.jobs.values():
            if job.steps is None:
                continue

            for step in job.steps:
                # Check for actions/checkout with PR head reference
                if step.uses and step.uses.startswith("actions/checkout"):
                    if step.with_config:
                        ref = step.with_config.get("ref", "")
                        if "pull_request.head" in ref:
                            return True
        return False

    def _has_explicit_permissions(self, workflow: GitHubWorkflow) -> bool:
        """Check if workflow has explicit permissions set."""
        # Check workflow-level permissions
        if workflow.permissions:
            return True

        # Check job-level permissions
        if workflow.jobs is not None:
            for job in workflow.jobs.values():
                if job.permissions:
                    return True
        return False

    def _get_permissions(self, workflow: GitHubWorkflow) -> Dict[str, Any]:
        """Get the permissions configuration from workflow."""
        permissions: Dict[str, Any] = {}

        # Workflow-level permissions
        if workflow.permissions:
            permissions.update(workflow.permissions)

        # Job-level permissions (overwrites workflow-level)
        if workflow.jobs is not None:
            for job in workflow.jobs.values():
                if job.permissions:
                    permissions.update(job.permissions)

        return permissions

    def _is_safe_permissions(self, permissions: Dict[str, Any]) -> bool:
        """Check if permissions are safe (no dangerous write access)."""
        # List of dangerous write permissions
        dangerous_write_permissions = [
            "contents",  # Can modify repository contents
            "metadata",  # Can modify repository metadata
            "packages",  # Can publish packages
            "pages",  # Can deploy to GitHub Pages
            "deployments",  # Can create deployments
            "security-events",  # Can create security events
        ]

        for perm, value in permissions.items():
            if perm in dangerous_write_permissions and value == "write":
                return False

        return True

    def test_pull_request_target_workflows_have_safe_permissions(self) -> None:
        """
        Test that all workflows using pull_request_target have explicit
        safe permissions to prevent pwn request vulnerabilities.
        """
        vulnerable_workflows: List[str] = []
        unsafe_permission_workflows: List[str] = []

        for workflow_path in self.workflow_files:
            workflow = self._load_workflow(workflow_path)

            if not self._has_pull_request_target(workflow):
                continue

            workflow_name = workflow_path.name

            # Check if it has untrusted code checkout (potential vulnerability)
            if self._has_untrusted_code_checkout(workflow):
                if not self._has_explicit_permissions(workflow):
                    vulnerable_workflows.append(workflow_name)
                else:
                    permissions = self._get_permissions(workflow)
                    if not self._is_safe_permissions(permissions):
                        unsafe_permission_workflows.append(
                            f"{workflow_name}: {permissions}"
                        )

        # Report findings
        error_messages: List[str] = []

        if vulnerable_workflows:
            error_messages.append(
                f"CRITICAL: Found {len(vulnerable_workflows)} workflows with pull_request_target "
                f"that checkout untrusted code without explicit permissions:\n"
                + "\n".join(f"  - {w}" for w in vulnerable_workflows)
                + "\n\nThis is a critical security vulnerability! These workflows can be exploited "
                "to gain repository write access through malicious PRs."
            )

        if unsafe_permission_workflows:
            error_messages.append(
                f"UNSAFE: Found {len(unsafe_permission_workflows)} workflows with pull_request_target "
                f"that have dangerous write permissions:\n"
                + "\n".join(f"  - {w}" for w in unsafe_permission_workflows)
                + "\n\nThese workflows should use minimal read-only permissions."
            )

        if error_messages:
            self.fail(
                "\n\n".join(error_messages)
                + "\n\nRecommended fix: Add explicit minimal permissions to these workflows:\n"
                "permissions:\n"
                "  contents: read\n"
                "  actions: read\n"
                "  id-token: write\n"
                "  pull-requests: read"
            )

    def test_no_workflow_uses_excessive_permissions(self) -> None:
        """
        Test that no workflow uses overly broad permissions like 'write-all'.
        """
        excessive_permission_workflows: List[str] = []

        for workflow_path in self.workflow_files:
            workflow = self._load_workflow(workflow_path)
            permissions = self._get_permissions(workflow)

            # Check for dangerous permission patterns
            for perm, value in permissions.items():
                if value in ["write-all", "admin"]:
                    excessive_permission_workflows.append(
                        f"{workflow_path.name}: {perm}={value}"
                    )

        if excessive_permission_workflows:
            self.fail(
                f"Found workflows with excessive permissions:\n"
                + "\n".join(f"  - {w}" for w in excessive_permission_workflows)
                + "\n\nUse minimal required permissions instead."
            )


if __name__ == "__main__":
    unittest.main()
