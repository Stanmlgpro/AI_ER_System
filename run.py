import subprocess
import sys

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python3 run.py <scenario_directory>")
        sys.exit(1)
        
    directory = sys.argv[1]
    print(f"Running scenario directory: {directory}")
    subprocess.run(["make"])
    subprocess.run(["./run", directory])
