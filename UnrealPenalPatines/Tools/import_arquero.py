"""Importa SOLO lo del arquero (equipo y camiseta). No toca el mapa, ni el piso, ni las gradas, ni ningún material.
Se corre dentro del editor: Tools > Execute Python Script... > Tools/import_arquero.py"""
import os
import unreal

PROJ = unreal.Paths.project_dir()
SRC = os.path.join(PROJ, "SourceAssets")
at = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary


def log(msg):
    unreal.log("[PenalPatines] " + msg)


def import_file(f, dest):
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", f)
    t.set_editor_property("destination_path", dest)
    t.set_editor_property("automated", True)
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("save", True)
    at.import_asset_tasks([t])


tex = os.path.join(SRC, "Textures", "Matias", "T_Arquero_BaseColor.png")
if os.path.exists(tex):
    import_file(tex, "/Game/Textures/Matias")
    t_ = eal.load_asset("/Game/Textures/Matias/T_Arquero_BaseColor")
    if t_:
        t_.set_editor_property("srgb", True)
        eal.save_loaded_asset(t_)
    log("Camiseta del arquero importada")
for name_ in ("SM_Polaina", "SM_Guante", "SM_Blocker", "SM_Careta"):
    f_ = os.path.join(SRC, "Models", name_ + ".glb")
    if os.path.exists(f_):
        import_file(f_, "/Game/Models")
        log("%s importado" % name_)
    else:
        log("Falta %s" % f_)
log("Listo. El mapa no se tocó.")
