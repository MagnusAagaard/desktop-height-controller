#!/usr/bin/env python3
"""
Run native tests with automatic coverage report generation.
Usage: python scripts/test_coverage.py
"""
import os
import subprocess
import sys

PROJECT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD_DIR = os.path.join(PROJECT_DIR, ".pio", "build", "native")
COVERAGE_DIR = os.path.join(BUILD_DIR, "coverage")


def run_tests():
    """Run PlatformIO native tests."""
    print("=" * 60)
    print("Running Native Tests with Coverage")
    print("=" * 60 + "\n")
    
    result = subprocess.run(
        ["pio", "test", "-e", "native"],
        cwd=PROJECT_DIR
    )
    return result.returncode == 0


def generate_coverage():
    """Generate coverage report from test data."""
    print("\n" + "=" * 60)
    print("Generating Code Coverage Report")
    print("=" * 60)
    
    # Check if coverage data exists
    test_dir = os.path.join(BUILD_DIR, "test")
    gcda_count = 0
    if os.path.exists(test_dir):
        for root, dirs, files in os.walk(test_dir):
            gcda_count += sum(1 for f in files if f.endswith(".gcda"))
    
    if gcda_count == 0:
        print("\nNo coverage data found (.gcda files)")
        return False
    
    print(f"\nFound {gcda_count} coverage data files")
    
    # Create coverage output directory
    os.makedirs(COVERAGE_DIR, exist_ok=True)
    
    coverage_info = os.path.join(COVERAGE_DIR, "coverage.info")
    coverage_filtered = os.path.join(COVERAGE_DIR, "coverage_filtered.info")
    html_dir = os.path.join(COVERAGE_DIR, "html")
    
    try:
        # Capture coverage data
        print("\n1. Capturing coverage data...")
        subprocess.run([
            "lcov", "--capture",
            "--directory", test_dir,
            "--output-file", coverage_info,
            "--quiet"
        ], check=True, capture_output=True)
        
        # Filter to only include test files
        print("2. Filtering to test files...")
        subprocess.run([
            "lcov", "--extract", coverage_info,
            os.path.join(PROJECT_DIR, "test", "*"),
            "--output-file", coverage_filtered,
            "--quiet"
        ], check=True, capture_output=True)
        
        # Generate HTML report
        print("3. Generating HTML report...")
        subprocess.run([
            "genhtml", coverage_filtered,
            "--output-directory", html_dir,
            "--title", "Desktop Height Controller - Test Coverage",
            "--quiet"
        ], check=True, capture_output=True)
        
        # Print summary
        print("\n" + "-" * 60)
        print("Coverage Summary:")
        print("-" * 60)
        result = subprocess.run(
            ["lcov", "--summary", coverage_filtered],
            capture_output=True, text=True
        )
        
        for line in result.stdout.split("\n"):
            if "lines" in line or "functions" in line:
                print(line.strip())
        
        print("\n" + "-" * 60)
        print(f"HTML Report: {html_dir}/index.html")
        print("-" * 60 + "\n")
        return True
        
    except subprocess.CalledProcessError as e:
        print(f"\nError generating coverage: {e}")
        print("Make sure lcov is installed: sudo apt install lcov")
        return False
    except FileNotFoundError:
        print("\nlcov not found. Install with: sudo apt install lcov")
        return False


def main():
    tests_passed = run_tests()
    
    if tests_passed:
        generate_coverage()
    else:
        print("\nTests failed - skipping coverage report")
        sys.exit(1)


if __name__ == "__main__":
    main()
