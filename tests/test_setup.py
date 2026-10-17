import hashlib
import importlib.util
import json
from pathlib import Path, PureWindowsPath
import tempfile
import unittest
from unittest.mock import patch
from types import SimpleNamespace

SPEC = importlib.util.spec_from_file_location('installer', Path(__file__).parents[1] / 'builder' / 'setup.py')
installer = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(installer)

class SetupTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def games(self):
        gta, er = self.root/'Grand Theft Auto V', self.root/'ELDEN RING'/'Game'
        gta.mkdir(); er.mkdir(parents=True)
        for name in installer.GTA_REQUIRED: (gta/name).write_bytes(b'owned-test-fixture')
        for name in installer.ER_REQUIRED: (er/name).write_bytes(b'owned-test-fixture')
        return gta, er

    def test_parent_elden_ring_directory_is_resolved(self):
        gta, er = self.games()
        self.assertEqual(installer.resolve_games(gta, er.parent), (gta.resolve(), er.resolve()))

    def test_missing_game_file_is_an_actionable_error(self):
        gta, er = self.games(); (gta/'GTA5.exe').unlink()
        with self.assertRaisesRegex(installer.SetupError, 'GTA5.exe'):
            installer.resolve_games(gta, er)

    def test_wrong_executable_rejected_before_writes(self):
        gta, er = self.games()
        before = sorted(self.root.rglob('*'))
        with self.assertRaisesRegex(installer.SetupError, 'unsupported'):
            installer.validate_executable(er/'eldenring.exe', '0'*64)
        self.assertEqual(sorted(self.root.rglob('*')), before)

    def test_valid_executable_accepted(self):
        _, er = self.games()
        expected = hashlib.sha256(b'owned-test-fixture').hexdigest()
        self.assertEqual(installer.validate_executable(er/'eldenring.exe', expected), expected)

    def test_platform_fingerprint_keeps_exact_corruption_checks(self):
        stage = self.root/'stage'; stage.mkdir()
        file = stage/'animation.bin'; file.write_bytes(b'windows-animation')
        digest = hashlib.sha256(file.read_bytes()).hexdigest()
        requirement = {'path':'animation.bin', 'bytes':17,
                       'sha256':hashlib.sha256(b'mac-animation').hexdigest(),
                       'platform_sha256':{'Windows':digest}}
        with patch.object(installer.platform, 'system', return_value='Windows'):
            self.assertEqual(installer.validate_payload(stage,[requirement])['animation.bin']['sha256'],digest)
            file.write_bytes(b'corrupt-animation')
            with self.assertRaisesRegex(installer.SetupError,'differs'):
                installer.validate_payload(stage,[requirement])
        file.write_bytes(b'windows-animation')
        with patch.object(installer.platform, 'system', return_value='Darwin'):
            with self.assertRaisesRegex(installer.SetupError,'differs'):
                installer.validate_payload(stage,[requirement])

    def test_all_fingerprint_mismatches_are_reported_together(self):
        stage = self.root/'stage'; stage.mkdir()
        for name in ('car.bin','animation.bin'):(stage/name).write_bytes(b'fixture')
        requirements = [{'path':name,'sha256':'0'*64} for name in ('car.bin','animation.bin')]
        with self.assertRaises(installer.SetupError) as error:
            installer.validate_payload(stage, requirements)
        self.assertIn('car.bin',str(error.exception))
        self.assertIn('animation.bin',str(error.exception))

    def test_output_cannot_overlap_game_or_package(self):
        gta, er = self.games(); package = self.root/'package'; package.mkdir()
        for output in [gta, gta/'mod', er.parent, self.root, package/'install']:
            with self.subTest(output=output), self.assertRaises(installer.SetupError):
                installer.validate_output(output, [gta, er, package])
        installer.validate_output(self.root/'separate install', [gta, er, package])

    def test_symlink_output_into_game_rejected(self):
        gta, er = self.games(); alias = self.root/'alias'; alias.symlink_to(gta, target_is_directory=True)
        with self.assertRaises(installer.SetupError):
            installer.validate_output(alias/'mod', [gta, er])

    def test_windows_arguments_preserve_spaces_and_metacharacters(self):
        args = installer.launch_arguments('Windows', PureWindowsPath('C:/Tools/me3.exe'),
            PureWindowsPath('D:/Games & More/ELDEN RING/Game/eldenring.exe'),
            PureWindowsPath('D:/igobygod/profile.me3'))
        self.assertIn('D:\\Games & More\\ELDEN RING\\Game\\eldenring.exe', args)
        self.assertNotIn('--skip-steam-init', args)
        self.assertEqual(args[-2:], ['--heap-size', '4096'])

    def test_crossover_arguments_use_selected_bottle(self):
        args = installer.launch_arguments('Darwin', Path('/tools/me3.exe'), Path('/games/ER/eldenring.exe'),
            Path('/mods/profile.me3'), wine=Path('/CrossOver/bin/wine'), bottle=Path('/bottles/Game Bottle'))
        self.assertEqual(args[:3], ['/CrossOver/bin/wine', '--bottle', '/bottles/Game Bottle'])
        self.assertIn('Z:\\games\\ER\\eldenring.exe', args)

    def test_missing_assets_refuse_finalization(self):
        stage=self.root/'stage'; stage.mkdir()
        with self.assertRaisesRegex(installer.SetupError, 'missing'):
            installer.validate_payload(stage, [{'path':'data/model.bin','min_bytes':12}])

    def test_payload_traversal_rejected(self):
        stage=self.root/'stage'; stage.mkdir()
        for name in ['../outside', '/outside', 'data/../../outside', 'C:\\outside', 'data\\..\\outside']:
            with self.subTest(name=name), self.assertRaises(installer.SetupError):
                installer.validate_payload(stage, [{'path':name,'min_bytes':1}])

    def test_existing_install_never_overwritten(self):
        staged=self.root/'staged'; staged.mkdir(); (staged/'new').write_text('new')
        final=self.root/'installed'; final.mkdir(); (final/'old').write_text('old')
        with self.assertRaises(installer.SetupError): installer.commit_install(staged, final)
        self.assertEqual((final/'old').read_text(),'old')
        self.assertFalse((final/'new').exists())

    def test_atomic_install_moves_complete_stage(self):
        staged=self.root/'staged'; staged.mkdir(); (staged/'new').write_text('new')
        final=self.root/'installed'; installer.commit_install(staged, final)
        self.assertEqual((final/'new').read_text(),'new'); self.assertFalse(staged.exists())


    def test_running_wine_game_is_detected_on_macos(self):
        output = 'Z:\\Games\\ELDEN RING\\Game\\eldenring.exe\n/usr/bin/other\n'
        with patch.object(installer.platform, 'system', return_value='Darwin'), \
             patch.object(installer.subprocess, 'run', return_value=SimpleNamespace(stdout=output)):
            with self.assertRaisesRegex(installer.SetupError, 'close Elden Ring'):
                installer.ensure_game_closed()

    def test_process_detection_ignores_launcher_and_other_game(self):
        output = 'Z:\\Tools\\me3-launcher.exe\n/usr/bin/other\n'
        with patch.object(installer.platform, 'system', return_value='Darwin'), \
             patch.object(installer.subprocess, 'run', return_value=SimpleNamespace(stdout=output)):
            installer.ensure_game_closed()

    def test_check_and_build_are_mutually_exclusive(self):
        with self.assertRaises(SystemExit) as caught:
            installer.main(['--check', '--build'])
        self.assertEqual(caught.exception.code, 2)

    def test_broken_symlink_destination_is_preserved(self):
        staged=self.root/'stage'; staged.mkdir()
        final=self.root/'destination'; final.symlink_to(self.root/'missing')
        with self.assertRaises(installer.SetupError):
            installer.commit_install(staged,final)
        self.assertTrue(final.is_symlink())
        self.assertTrue(staged.is_dir())


    def test_asset_builder_uses_prepared_python_and_separate_scratch(self):
        args=installer.asset_arguments(Path('/package'),Path('/tools'),Path('/scratch'),
            Path('/gta'),Path('/er'),java='java21',wine=Path('/wine'),bottle=Path('/bottle'))
        self.assertEqual(args[0], str(Path('/tools/python/bin/python')))
        self.assertEqual(args[args.index('--output')+1], '/scratch')
        self.assertIn('--wine',args)
        self.assertIn('--bottle',args)
        self.assertEqual(args[args.index('--java')+1], 'java21')

    def test_validated_install_keeps_every_required_asset(self):
        stage=self.root/'stage';stage.mkdir();(stage/'data').mkdir()
        (stage/'data/a').write_bytes(b'one');(stage/'data/b').write_bytes(b'two')
        required=[{'path':'data/a','bytes':3},{'path':'data/b','bytes':3}]
        hashes=installer.validate_payload(stage,required)
        (stage/'data/b').write_bytes(b'bad')
        with self.assertRaisesRegex(installer.SetupError,'differs'):
            installer.validate_payload(stage,[{'path':k,**v} for k,v in hashes.items()])


    def test_folder_picker_display_error_falls_back_with_actionable_message(self):
        class TclFailure(Exception): pass
        fake_tk=SimpleNamespace(filedialog=SimpleNamespace(), TclError=TclFailure, Tk=lambda: (_ for _ in ()).throw(TclFailure('no display')))
        with patch.dict('sys.modules', {'tkinter':fake_tk,'tkinter.filedialog':SimpleNamespace()}), \
             patch.object(installer.sys.stdin, 'isatty',return_value=False):
            with self.assertRaisesRegex(installer.SetupError,'Folder picker unavailable'):
                installer.choose_directory('Choose game')


    def test_build_verified_candidate_does_not_require_gameplay_release_claim(self):
        gta,er=self.games()
        manifest={'release_ready':False,'elden_ring_sha256':hashlib.sha256(b'owned-test-fixture').hexdigest(),
                  'platforms':{'Darwin':{'build_ready':True,'gameplay_verified':False}}}
        with patch.object(installer,'read_manifest',return_value=manifest), \
             patch.object(installer.platform,'system',return_value='Darwin'):
            with self.assertRaisesRegex(installer.SetupError,'Specify --output'):
                installer.main(['--build','--gta',str(gta),'--elden-ring',str(er)])

if __name__ == '__main__': unittest.main()
