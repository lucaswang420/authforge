import os
import shutil

MAPPING = {
    "ValidatorTest.cc": "unit/validation/ValidatorTest.cc",
    "ValidationHelperTest.cc": "unit/validation/ValidationHelperTest.cc",
    "SubjectMappingTest.cc": "unit/subject/SubjectMappingTest.cc",
    "StringUtilsTest.cc": "unit/utils/StringUtilsTest.cc",
    "CryptoUtilsTest.cc": "unit/utils/CryptoUtilsTest.cc",
    "ConfigManagerTest.cc": "unit/config/ConfigManagerTest.cc",
    "MemoryStorageTest.cc": "integration/storage/MemoryStorageTest.cc",
    "PostgresStorageTest.cc": "integration/storage/PostgresStorageTest.cc",
    "RedisStorageTest.cc": "integration/storage/RedisStorageTest.cc",
    "AdvancedStorageTest.cc": "integration/storage/AdvancedStorageTest.cc",
    "OAuth2ControllerTest.cc": "integration/controller/OAuth2ControllerTest.cc",
    "AdminControllerTest.cc": "integration/controller/AdminControllerTest.cc",
    "OAuth2PluginTest.cc": "integration/plugin/OAuth2PluginTest.cc",
    "MetricsPluginTest.cc": "integration/plugin/MetricsPluginTest.cc",
    "LoginFlowTest.cc": "e2e/oauth2_flows/LoginFlowTest.cc",
    "OAuth2FlowE2ETest.cc": "e2e/oauth2_flows/OAuth2FlowE2ETest.cc",
    "IntegrationE2ETest.cc": "e2e/oauth2_flows/IntegrationE2ETest.cc",
    "SecurityTest.cc": "security/injection/SecurityTest.cc",
    "SecurityValidationTest.cc": "security/injection/SecurityValidationTest.cc",
    "PerformanceBenchmark.cc": "performance/benchmark/PerformanceBenchmark.cc",
}

def main():
    base_dir = "OAuth2Backend/test"
    
    for old_file, new_rel_path in MAPPING.items():
        # Search for old_file recursively in base_dir
        found_path = None
        for root, dirs, files in os.walk(base_dir):
            if old_file in files:
                found_path = os.path.join(root, old_file)
                break
        
        if found_path:
            dest_path = os.path.join(base_dir, new_rel_path)
            os.makedirs(os.path.dirname(dest_path), exist_ok=True)
            print(f"Moving {found_path} to {dest_path}")
            shutil.move(found_path, dest_path)
        else:
            print(f"Warning: {old_file} not found")

if __name__ == "__main__":
    main()
