import subprocess
import os
import sys
import fnmatch
import argparse

def run(pattern=None, is_release=False):
    if is_release:
        configuration = "Release"
    else:
        configuration = "Debug"

    binary_path = f"./x64/{configuration}/cpplox.exe"

    # testsディレクトリ内の.loxファイルのパスを取得
    tests_directory = './tests'
    lox_files = [file for file in os.listdir(tests_directory) if file.endswith('.lox')]

    if pattern:
        lox_files = fnmatch.filter(lox_files, f"*{pattern}*")

    lox_files.sort()  # ファイルをソート

    test_succeed = True

    results = []

    # .loxファイルを順番に実行
    for lox_file in lox_files:
        file_path = os.path.join(tests_directory, lox_file)
        print(f"============================================")
        print(f"run: {lox_file}")

        command = [binary_path, file_path]
        process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

        return_code = None
        while return_code == None:
            return_code = process.poll()

            for line in process.stdout:
                print(line.decode(), end='')

            for line in process.stderr:
                print(line.decode(), end='')
        
        if return_code == 0:
            print(f"[PASS] {lox_file}")
            results.append({ "file": lox_file, "result": True})
        else:
            print(f"[FAIL] {lox_file}")
            results.append({ "file": lox_file, "result": False})
            test_succeed = False

    print(f"============================================")
    print(f"[Result] {test_succeed}")
    for r in results:
        print(f"- {r["file"]}: {r["result"]}")
    
    if test_succeed:
        exit(0)
    else:
        exit(1)

def main():
    parser = argparse.ArgumentParser(description='Lox test benchmark')
    parser.add_argument('--release', action='store_true', help='Release mode')
    parser.add_argument('--pattern', type=str, help='Pattern argument', default="", required=False)

    args = parser.parse_args()

    run(args.pattern, args.release)

if __name__ == "__main__":
    main()
    pattern_arg = None
    if len(sys.argv) > 1:
        pattern_arg = sys.argv[1]
    run(pattern_arg)