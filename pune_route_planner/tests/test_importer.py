import importlib.util,pathlib,unittest
p=pathlib.Path(__file__).parents[1]/'tools'/'import_osm.py'
spec=importlib.util.spec_from_file_location('import_osm',p);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
e=importlib.util.spec_from_file_location('enrich',p.parent/'enrich_metrics.py');en=importlib.util.module_from_spec(e);e.loader.exec_module(en)
class EnrichTests(unittest.TestCase):
 def test_crowd_curve(self):
  m={'peak':[16,18],'popularity':9,'open':8,'close':18}
  wd=en.crowd_curve(m,False);we=en.crowd_curve(m,True)
  self.assertEqual(len(wd),24);self.assertEqual(wd[7],0);self.assertEqual(wd[18],0)
  self.assertTrue(all(0<=x<=10 for x in wd+we));self.assertGreaterEqual(wd[16],wd[9]);self.assertGreaterEqual(we[16],wd[16])
 def test_meta_complete(self):
  import json
  meta=json.load(open(p.parent/'attractions_meta.json',encoding='utf-8'))
  for code,m in meta['attractions'].items():self.assertTrue(0<m['open']<m['close']<=24 and m['visit_minutes']>0 and 0<=m['popularity']<=10,code)
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
