import subprocess
import os

def run():
    binary_path = "./x64/Debug/cpplox.exe"

    # testsディレクトリ内の.loxファイルのパスを取得
    tests_directory = './tests'
    lox_files = [file for file in os.listdir(tests_directory) if file.endswith('.lox')]
    lox_files.sort()  # ファイルをソート

    test_failed = False

    # .loxファイルを順番に実行
    for lox_file in lox_files:
        file_path = os.path.join(tests_directory, lox_file)
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
        else:
            print(f"[FAIL] {lox_file}")
            test_failed = True
    
    if not test_failed:
        print(f"[Result] Success")
        exit(0)
    else:
        print(f"[Result] Failure")
        exit(1)

if __name__ == "__main__":
    run()