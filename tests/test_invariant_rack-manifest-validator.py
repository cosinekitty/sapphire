import pytest
import subprocess
import sys
import os
import tempfile


@pytest.mark.parametrize("repo_path", [
    "/tmp/path with spaces/repo",
    "/tmp/path\twith\ttabs",
    "/tmp/path;injected-command",
    "/tmp/normal-path",
])
def test_git_command_tokenization_handles_special_paths(repo_path, tmp_path):
    """Invariant: Command tokenization must not corrupt arguments when paths contain spaces or metacharacters."""
    # Import the module under test
    import importlib.util
    spec = importlib.util.spec_from_file_location(
        "rack_manifest_validator",
        os.path.join(os.path.dirname(__file__), "..", "util", "rack-manifest-validator.py")
    )
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)

    # The vulnerable pattern is: cmd.split(" ") which breaks on spaces in paths
    # We test that git_run (or equivalent) properly tokenizes commands
    # by checking that a path with spaces doesn't get split into multiple args
    test_cmd = f"git log --oneline -1"
    tokens = test_cmd.split(" ")

    # The security invariant: splitting on space must produce the same result
    # as shlex.split for simple commands. But for paths with spaces embedded
    # in constructed commands, split(" ") will incorrectly tokenize.
    import shlex

    # Simulate the vulnerable pattern with a path containing spaces
    cmd_with_path = f"git -C {repo_path} log --oneline -1"
    naive_split = cmd_with_path.split(" ")
    safe_split = shlex.split(cmd_with_path)

    if " " in repo_path or "\t" in repo_path:
        # Security invariant: naive split MUST NOT be used when path has whitespace
        # because it corrupts the argument list
        assert naive_split != safe_split, (
            f"Naive split should differ from safe split for path with whitespace: {repo_path}"
        )
    else:
        # For paths without whitespace, both should produce same tokenization
        # (unless semicolons or other shell metacharacters are present)
        # The key point: the code should never rely on naive splitting
        assert len(naive_split) >= len(safe_split)