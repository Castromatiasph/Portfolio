"""Se corre UNA vez dentro del editor de Unreal (Output Log -> cmd Python, o Tools > Execute Python Script):

    py "<ruta>/UnrealPenalPatines/Tools/setup_level.py"

Importa los modelos (arco, butaca, patín, careta, jugadores) y las texturas de SourceAssets/, crea el
material de parquet con clear coat, arma el mapa /Game/Maps/Pista con APPArena y asigna todo.
Está escrito de forma defensiva: si un paso falla, lo informa y sigue con el resto."""
import os
import unreal

PROJ = unreal.Paths.project_dir()
SRC = os.path.join(PROJ, "SourceAssets")
at = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary


def log(msg):
    unreal.log("[PenalPatines] " + msg)


def import_files(files, dest):
    tasks = []
    for f in files:
        t = unreal.AssetImportTask()
        t.set_editor_property("filename", f)
        t.set_editor_property("destination_path", dest)
        t.set_editor_property("automated", True)
        t.set_editor_property("replace_existing", True)
        t.set_editor_property("save", True)
        tasks.append(t)
    at.import_asset_tasks(tasks)
    return [p for t in tasks for p in t.get_editor_property("imported_object_paths")]


def first_asset(folder, needle, cls):
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    for a in reg.get_assets_by_path(folder, recursive=True):
        if needle.lower() in str(a.asset_name).lower() and str(a.asset_class_path.asset_name) == cls:
            return a.get_asset()
    return None


# 1) modelos ------------------------------------------------------------
models = sorted(os.path.join(SRC, "Models", f) for f in os.listdir(os.path.join(SRC, "Models")) if f.endswith(".gltf"))
for m in models:
    try:
        log("Importando %s -> %s" % (os.path.basename(m), import_files([m], "/Game/Models")))
    except Exception as e:
        log("FALLÓ la importación de %s: %s" % (m, e))

# 2) texturas de parquet ------------------------------------------------
tex = {}
for key, fname, srgb in (("base", "T_Parquet_BaseColor.png", True), ("rough", "T_Parquet_Roughness.png", False), ("norm", "T_Parquet_Normal.png", False)):
    try:
        import_files([os.path.join(SRC, "Textures", fname)], "/Game/Textures")
        t = unreal.EditorAssetLibrary.load_asset("/Game/Textures/" + fname[:-4])
        t.set_editor_property("srgb", srgb)
        if key == "norm":
            t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        elif key == "rough":
            t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
        tex[key] = t
    except Exception as e:
        log("textura %s: %s" % (fname, e))

# 3) materiales ---------------------------------------------------------
def make_material(name, shading=None):
    path = "/Game/Materials/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    m = at.create_asset(name, "/Game/Materials", unreal.Material, unreal.MaterialFactoryNew())
    if shading:
        m.set_editor_property("shading_model", shading)
    return m


def expr(mat, cls, x, y):
    return mel.create_material_expression(mat, cls, x, y)


mats = {}
try:
    m = make_material("M_Parquet", unreal.MaterialShadingModel.MSM_CLEAR_COAT)
    uv = expr(m, unreal.MaterialExpressionTextureCoordinate, -900, 0)
    uv.set_editor_property("u_tiling", 7.0)   # 20 m / 2,88 m
    uv.set_editor_property("v_tiling", 7.0)
    for key, prop, y in (("base", unreal.MaterialProperty.MP_BASE_COLOR, -200), ("rough", unreal.MaterialProperty.MP_ROUGHNESS, 100), ("norm", unreal.MaterialProperty.MP_NORMAL, 400)):
        if key not in tex:
            continue
        ts = expr(m, unreal.MaterialExpressionTextureSample, -500, y)
        ts.set_editor_property("texture", tex[key])
        mel.connect_material_expressions(uv, "", ts, "UVs")
        mel.connect_material_property(ts, "RGB" if key != "rough" else "R", prop)
    cc = expr(m, unreal.MaterialExpressionConstant, -300, 700)
    cc.set_editor_property("r", 1.0)
    mel.connect_material_property(cc, "", unreal.MaterialProperty.MP_CLEAR_COAT)
    ccr = expr(m, unreal.MaterialExpressionConstant, -300, 800)
    ccr.set_editor_property("r", 0.06)
    mel.connect_material_property(ccr, "", unreal.MaterialProperty.MP_CLEAR_COAT_ROUGHNESS)
    mel.recompile_material(m)
    mats["floor"] = m
except Exception as e:
    log("M_Parquet: %s" % e)


def flat(name, color, rough, metal=0.0):
    m = make_material(name)
    c = expr(m, unreal.MaterialExpressionConstant3Vector, -400, -100)
    c.set_editor_property("constant", unreal.LinearColor(*color))
    mel.connect_material_property(c, "", unreal.MaterialProperty.MP_BASE_COLOR)
    r = expr(m, unreal.MaterialExpressionConstant, -400, 100)
    r.set_editor_property("r", rough)
    mel.connect_material_property(r, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mt = expr(m, unreal.MaterialExpressionConstant, -400, 250)
    mt.set_editor_property("r", metal)
    mel.connect_material_property(mt, "", unreal.MaterialProperty.MP_METALLIC)
    mel.recompile_material(m)
    return m


try:
    mc = make_material("M_Color")
    ec = expr(mc, unreal.MaterialExpressionVectorParameter, -500, -200)
    ec.set_editor_property("parameter_name", "Color")
    ec.set_editor_property("default_value", unreal.LinearColor(0.5, 0.5, 0.5, 1))
    mel.connect_material_property(ec, "", unreal.MaterialProperty.MP_BASE_COLOR)
    er = expr(mc, unreal.MaterialExpressionScalarParameter, -500, 50)
    er.set_editor_property("parameter_name", "Roughness")
    er.set_editor_property("default_value", 0.5)
    mel.connect_material_property(er, "", unreal.MaterialProperty.MP_ROUGHNESS)
    ee = expr(mc, unreal.MaterialExpressionVectorParameter, -500, 250)
    ee.set_editor_property("parameter_name", "Emissive")
    ee.set_editor_property("default_value", unreal.LinearColor(0, 0, 0, 1))
    mel.connect_material_property(ee, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(mc)
    unreal.EditorAssetLibrary.save_loaded_asset(mc)
except Exception as e:
    log("M_Color: %s" % e)

try:
    mats["board"] = flat("M_Valla", (0.80, 0.82, 0.86, 1), 0.45)
    mats["line"] = flat("M_Linea", (0.90, 0.91, 0.93, 1), 0.4)
    mats["stand"] = flat("M_Grada", (0.05, 0.055, 0.07, 1), 0.85)
except Exception as e:
    log("materiales planos: %s" % e)

# 4) mapa ---------------------------------------------------------------
unreal.EditorLevelLibrary.new_level("/Game/Maps/Pista")
arena_cls = getattr(unreal, "PPArena", None)
if arena_cls is None:
    unreal.log_error("[PenalPatines] No encuentro la clase PPArena: compilá el proyecto (Build) antes de correr este script.")
else:
    arena = unreal.EditorLevelLibrary.spawn_actor_from_class(arena_cls, unreal.Vector(0, 0, 0))
    arena.set_actor_label("Pista")
    for prop, key in (("floor_material", "floor"), ("board_material", "board"), ("line_material", "line"), ("stand_material", "stand")):
        if key in mats:
            arena.set_editor_property(prop, mats[key])
    goal = first_asset("/Game/Models", "arco", "StaticMesh")
    if goal:
        arena.set_editor_property("goal_mesh", goal)
    else:
        log("No encontré SM_Arco importado; se usa el arco de formas básicas.")
    seat = first_asset("/Game/Models", "butaca", "StaticMesh") or first_asset("/Game/Models", "seat", "StaticMesh")
    if seat:
        arena.set_editor_property("seat_mesh", seat)
    unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(-540, 0, 50))
    unreal.EditorLevelLibrary.save_current_level()
    log("Mapa /Game/Maps/Pista listo. Dale Play.")
