import os
import unittest
from typing import List

from ci.paths import PROJECT_ROOT


SRC_ROOT = PROJECT_ROOT / "src"


class TestJsonNamespace(unittest.TestCase):
    """Test that JSON files are properly under the fl namespace."""

    def check_namespace_in_file(self, file_path: str) -> List[str]:
        """Check if a file has proper fl namespace usage."""
        failings = []
        
        try:
            with open(file_path, "r", encoding="utf-8") as f:
                content = f.read()
                
            # Check if namespace fl is properly declared
            if "namespace fl {" not in content:
                failings.append(f"Missing 'namespace fl {{' in {file_path}")
                
            # Check if namespace is properly closed
            if "} // namespace fl" not in content:
                failings.append(f"Missing '} // namespace fl' in {file_path}")
                
            # Check for any JsonValue or Json declarations outside of namespace
            lines = content.split('\n')
            in_namespace = False
            
            for i, line in enumerate(lines, 1):
                # Skip comments and empty lines
                stripped_line = line.strip()
                if not stripped_line or stripped_line.startswith('//'):
                    continue
                    
                # Track namespace state
                if "namespace fl {" in line:
                    in_namespace = True
                    continue
                    
                if "} // namespace fl" in line:
                    in_namespace = False
                    continue
                    
                # Check for Json-related declarations outside namespace
                if not in_namespace and ("struct JsonValue" in line or 
                                        "class Json" in line or
                                        "JsonValue& get_null_value()" in line):
                    failings.append(
                        f"Found Json declaration outside namespace in {file_path} at line {i}: {line.strip()}"
                    )
                    
        except Exception as e:
            failings.append(f"Error reading {file_path}: {str(e)}")
            
        return failings

    def test_json_namespace(self) -> None:
        """Test that JSON files are properly under the fl namespace."""
        json_files = [
            os.path.join(SRC_ROOT, "fl", "json.h"),
            os.path.join(SRC_ROOT, "fl", "json.cpp"),
        ]
        
        all_failings = []
        for file_path in json_files:
            if os.path.exists(file_path):
                failings = self.check_namespace_in_file(file_path)
                all_failings.extend(failings)
            else:
                all_failings.append(f"File not found: {file_path}")
        
        if all_failings:
            msg = f"Found {len(all_failings)} namespace issue(s):\n" + "\n".join(all_failings)
            self.fail(msg)
        else:
            print("All JSON files are properly under the fl namespace.")


if __name__ == "__main__":
    unittest.main()