import importlib.util,pathlib,unittest
p=pathlib.Path(__file__).parents[1]/'tools'/'import_osm.py'
spec=importlib.util.spec_from_file_location('import_osm',p);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
class Tests(unittest.TestCase):
 def test_access(self):
  for tags in [{'highway':'footway'},{'highway':'construction'},{'highway':'service','access':'private'},{'highway':'residential','access':'destination'},{'highway':'residential','motorcar:conditional':'yes @ (Mo-Fr)'},{'highway':'residential','motorcar:forward':'no'},{'highway':'residential','oneway:motor_vehicle':'no'},{'highway':'residential','oneway':'reversible'}]:self.assertFalse(m.permitted(tags))
  self.assertTrue(m.permitted({'highway':'residential','access':'no','motorcar':'yes'}))
  self.assertTrue(m.permitted({'highway':'residential','oneway':'-1'}))
 def test_geometry(self):
  self.assertAlmostEqual(m.km([73.85,18.5],[73.85,18.5]),0)
  self.assertGreater(m.km([73.85,18.5],[73.85,18.501]),.11)
  self.assertFalse(m.inside([0,0]));self.assertTrue(m.inside([73.85,18.5]))
if __name__=='__main__':unittest.main()
