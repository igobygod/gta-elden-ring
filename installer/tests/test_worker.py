import pathlib, unittest, subprocess, sys, tempfile
class WorkerTests(unittest.TestCase):
 def test_worker_preserves_arguments_and_failure_exit(self):
  worker=pathlib.Path(__file__).parents[1]/'windows_worker.py'
  with tempfile.TemporaryDirectory() as temp:
   script=pathlib.Path(temp)/'script with spaces.py'
   script.write_text('import sys\nassert sys.argv[1:] == ["hello world", "quote\\\"inside"]\nraise SystemExit(7)\n')
   result=subprocess.run([sys.executable,str(worker),str(script),'hello world','quote"inside'],capture_output=True,text=True)
   self.assertEqual(result.returncode,7,result.stderr)
if __name__=='__main__':unittest.main()
