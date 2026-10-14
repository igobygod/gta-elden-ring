import importlib.util
from pathlib import Path
import tempfile
import unittest
import zipfile

class BootstrapTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        spec=importlib.util.spec_from_file_location('bootstrap', Path(__file__).parents[1]/'builder'/'bootstrap.py')
        cls.module=importlib.util.module_from_spec(spec);spec.loader.exec_module(cls.module)

    def test_zip_traversal_is_rejected_before_extraction(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);archive=root/'bad.zip';out=root/'out'
            with zipfile.ZipFile(archive,'w') as z:
                z.writestr('valid.txt','safe');z.writestr('../escaped.txt','bad')
            with self.assertRaises(ValueError):self.module.extract_zip(archive,out)
            self.assertFalse((root/'escaped.txt').exists())
            self.assertFalse((out/'valid.txt').exists())

    def test_valid_zip_is_extracted_without_replacing_existing_output(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);archive=root/'good.zip';out=root/'out'
            with zipfile.ZipFile(archive,'w') as z:z.writestr('nested/data.txt','ok')
            self.module.extract_zip(archive,out)
            self.assertEqual((out/'nested/data.txt').read_text(),'ok')
            with self.assertRaises(FileExistsError):self.module.extract_zip(archive,out)

    def test_download_hash_mismatch_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            p=Path(temp)/'download';p.write_bytes(b'wrong')
            with self.assertRaisesRegex(ValueError,'SHA-256'):self.module.verify_hash(p,'0'*64)

    def test_partial_archive_directory_is_not_accepted(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);archive=root/'archive.zip';out=root/'out';out.mkdir()
            with zipfile.ZipFile(archive,'w') as z:
                z.writestr('one','1');z.writestr('two','2')
            (out/'one').write_text('1')
            with self.assertRaisesRegex(ValueError,'incomplete'):
                self.module.verify_extraction(archive,out)
