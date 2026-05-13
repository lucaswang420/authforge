import os
import re
import sys

# Standard naming mapping (Examples from the optimization plan)
MAPPING = {
    "ValidatorTest": "Unit_P0_Validation_Validator",
    "ValidationHelperTest": "Unit_P0_Validation_Helper",
    "SubjectMappingTest": "Unit_P0_Subject_Mapping",
    "MemoryStorageTest": "Integration_P1_Storage_Memory",
    "PostgresStorageTest": "Integration_P0_Storage_Postgres",
    "RedisStorageTest": "Integration_P1_Storage_Redis",
    "OAuth2ControllerTest": "Integration_P0_OAuth2_Controller",
    "AdminControllerTest": "Integration_P1_Admin_Controller",
    "OAuth2PluginTest": "Integration_P0_Plugin_OAuth2",
    "LoginFlowTest": "E2E_P0_Auth_LoginFlow",
}

def migrate_file(file_path):
    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()

    # 1. Replace TEST(Suite, Name) with DROGON_TEST(Name) or MappedName
    # Pattern: TEST\s*\(\s*(\w+)\s*,\s*(\w+)\s*\)
    def test_replacer(match):
        suite = match.group(1)
        name = match.group(2)
        
        # Try to use mapping
        base = MAPPING.get(suite, suite)
        new_name = f"{base}_{name}"
        
        print(f"  Mapping: TEST({suite}, {name}) -> DROGON_TEST({new_name})")
        return f"DROGON_TEST({new_name})"

    new_content = re.sub(r'TEST\s*\(\s*(\w+)\s*,\s*(\w+)\s*\)', test_replacer, content)

    # 2. Add header if missing
    if "DROGON_TEST" in new_content and "drogon/drogon_test.h" not in new_content:
        new_content = "#include <drogon/drogon_test.h>\n" + new_content

    if new_content != content:
        with open(file_path, 'w', encoding='utf-8') as f:
            f.write(new_content)
        print(f"Updated {file_path}")

def main():
    test_dir = "OAuth2Backend/test"
    if not os.path.exists(test_dir):
        print(f"Directory {test_dir} not found")
        return

    for root, dirs, files in os.walk(test_dir):
        # Skip new hierarchical directories for now to avoid double processing if they contain files
        if any(x in root for x in ["unit", "integration", "e2e", "security", "performance"]):
            continue
            
        for file in files:
            if file.endswith(".cc") and file != "test_main.cc":
                print(f"Processing {file}...")
                migrate_file(os.path.join(root, file))

if __name__ == "__main__":
    main()
