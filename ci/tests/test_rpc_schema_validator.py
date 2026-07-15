import pytest

from ci.rpc_schema_validator import RpcSchemaValidator, parse_rpc_schema


def test_parse_compact_fastled_schema() -> None:
    schema = parse_rpc_schema(
        {
            "schema": [
                ["ping", "json", [], "sync"],
                ["echo", "int", [["value", "int"]], "sync"],
            ]
        }
    )

    assert schema.jsonrpc == "2.0"
    assert list(schema.methods) == ["ping", "echo"]
    assert schema.methods["echo"].returnType == "int"
    assert schema.methods["echo"].params[0].name == "value"
    assert schema.methods["echo"].params[0].type == "int"


def test_parse_expanded_legacy_schema() -> None:
    schema = parse_rpc_schema(
        {
            "jsonrpc": "2.0",
            "methods": {
                "ping": {
                    "returnType": "json",
                    "params": [],
                }
            },
        }
    )

    assert schema.methods["ping"].returnType == "json"


@pytest.mark.parametrize(
    "result",
    [
        {},
        {"schema": "not-a-list"},
        {"schema": [["missing-params", "json"]]},
        {"schema": [["bad-params", "json", "not-a-list"]]},
        {"schema": [["duplicate", "json", []], ["duplicate", "json", []]]},
    ],
)
def test_parse_rejects_malformed_compact_schema(result: object) -> None:
    with pytest.raises(ValueError):
        parse_rpc_schema(result)


def _validator_for(result: object) -> RpcSchemaValidator:
    validator = object.__new__(RpcSchemaValidator)
    validator.schema = parse_rpc_schema(result)
    return validator


@pytest.mark.parametrize("payload", [[], {}, [1, 2], {"driver": "PIO"}])
def test_opaque_json_parameter_accepts_any_payload(payload: object) -> None:
    validator = _validator_for(
        {"schema": [["runSingleTest", "unknown", [["arg0", "unknown"]], "async"]]}
    )

    validator.validate_request("runSingleTest", payload)


@pytest.mark.parametrize("payload", [None, [], {}])
def test_no_parameter_method_accepts_empty_payload(payload: object) -> None:
    validator = _validator_for({"schema": [["ping", "json", [], "sync"]]})

    validator.validate_request("ping", payload)


def test_expanded_typed_schema_still_checks_parameter_names() -> None:
    validator = _validator_for(
        {
            "jsonrpc": "2.0",
            "methods": {
                "echo": {
                    "returnType": "int",
                    "params": [{"name": "value", "type": "int"}],
                }
            },
        }
    )

    validator.validate_request("echo", {"value": 42})
    with pytest.raises(ValueError, match="Missing required parameters"):
        validator.validate_request("echo", {"wrong": 42})
