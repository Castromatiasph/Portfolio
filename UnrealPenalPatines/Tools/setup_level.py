"""Se corre UNA vez dentro del editor de Unreal (menú Tools > Execute Python Script...):

    Tools/setup_level.py

Importa texturas (parquet, publicidad) y sonidos, crea el material /Game/Materials/M_Color (con color, textura,
rugosidad, emisivo y clear coat como parámetros) y arma el mapa /Game/Maps/Pista con APPArena.
Cada paso está protegido: si uno falla lo informa en el Output Log y sigue con el resto.

IMPORT_MODELS = True importa además los modelos 3D originales (arco, patín, careta, tribuna y los dos jugadores
con esqueleto). No hacen falta para jugar: sirven si querés reemplazar el cuerpo provisorio por un personaje."""
import os
import unreal

IMPORT_MODELS = False

PROJ = unreal.Paths.project_dir()
SRC = os.path.join(PROJ, "SourceAssets")
at = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary


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


def prop(*names):
    """MaterialProperty por nombre (los nombres cambian entre versiones)."""
    for n in names:
        if hasattr(unreal.MaterialProperty, n):
            return getattr(unreal.MaterialProperty, n)
    raise AttributeError("MaterialProperty: " + " / ".join(names))


# 1) texturas -----------------------------------------------------------------
tex = {}
for key, fname, srgb, comp in (
    ("base", "T_Parquet_BaseColor.png", True, None),
    ("ads", "T_Publicidad.png", True, None),
):
    try:
        import_files([os.path.join(SRC, "Textures", fname)], "/Game/Textures")
        t = eal.load_asset("/Game/Textures/" + fname[:-4])
        t.set_editor_property("srgb", srgb)
        eal.save_loaded_asset(t)
        tex[key] = t
    except Exception as e:
        log("textura %s: %s" % (fname, e))

# 2) sonidos ------------------------------------------------------------------
try:
    wavs = sorted(os.path.join(SRC, "Audio", f) for f in os.listdir(os.path.join(SRC, "Audio")) if f.endswith(".wav"))
    import_files(wavs, "/Game/Audio")
    amb = eal.load_asset("/Game/Audio/S_Ambiente")
    amb.set_editor_property("looping", True)
    eal.save_loaded_asset(amb)
    log("Sonidos importados: %d" % len(wavs))
except Exception as e:
    log("sonidos: %s" % e)

# 3) material universal M_Color --------------------------------------------------
try:
    path = "/Game/Materials/M_Color"
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    m = at.create_asset("M_Color", "/Game/Materials", unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_CLEAR_COAT)

    def node(cls, x, y, **kw):
        n = mel.create_material_expression(m, cls, x, y)
        for k, v in kw.items():
            n.set_editor_property(k, v)
        return n

    uv = node(unreal.MaterialExpressionTextureCoordinate, -1100, 0)
    tiling = node(unreal.MaterialExpressionScalarParameter, -1100, 150, parameter_name="Tiling", default_value=1.0)
    uvm = node(unreal.MaterialExpressionMultiply, -850, 50)
    mel.connect_material_expressions(uv, "", uvm, "A")
    mel.connect_material_expressions(tiling, "", uvm, "B")

    ts = node(unreal.MaterialExpressionTextureSampleParameter2D, -600, 0, parameter_name="Tex")
    white = eal.load_asset("/Engine/EngineResources/WhiteSquareTexture")
    if white:
        ts.set_editor_property("texture", white)
    mel.connect_material_expressions(uvm, "", ts, "UVs")

    col = node(unreal.MaterialExpressionVectorParameter, -600, 250, parameter_name="Color", default_value=unreal.LinearColor(0.5, 0.5, 0.5, 1))
    mul = node(unreal.MaterialExpressionMultiply, -300, 100)
    mel.connect_material_expressions(ts, "RGB", mul, "A")
    mel.connect_material_expressions(col, "", mul, "B")
    mel.connect_material_property(mul, "", prop("MP_BASE_COLOR"))

    rough = node(unreal.MaterialExpressionScalarParameter, -300, 350, parameter_name="Roughness", default_value=0.5)
    mel.connect_material_property(rough, "", prop("MP_ROUGHNESS"))
    emis = node(unreal.MaterialExpressionVectorParameter, -300, 500, parameter_name="Emissive", default_value=unreal.LinearColor(0, 0, 0, 1))
    mel.connect_material_property(emis, "", prop("MP_EMISSIVE_COLOR"))
    cc = node(unreal.MaterialExpressionScalarParameter, -300, 650, parameter_name="ClearCoat", default_value=0.0)
    mel.connect_material_property(cc, "", prop("MP_CUSTOM_DATA0", "MP_CLEAR_COAT"))
    ccr = node(unreal.MaterialExpressionConstant, -300, 800, r=0.06)
    mel.connect_material_property(ccr, "", prop("MP_CUSTOM_DATA1", "MP_CLEAR_COAT_ROUGHNESS"))
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    log("M_Color creado")
except Exception as e:
    log("M_Color: %s" % e)

# 4) modelos (opcional) ---------------------------------------------------------
if IMPORT_MODELS:
    mdir = os.path.join(SRC, "Models")
    for f in sorted(os.listdir(mdir)):
        if f.endswith(".gltf"):
            try:
                import_files([os.path.join(mdir, f)], "/Game/Models")
            except Exception as e:
                log("modelo %s: %s" % (f, e))

# 5) mapa ---------------------------------------------------------------------
unreal.EditorLevelLibrary.new_level("/Game/Maps/Pista")
arena_cls = getattr(unreal, "PPArena", None)
if arena_cls is None:
    unreal.log_error("[PenalPatines] No encuentro la clase PPArena: compilá el proyecto (Build) antes de correr este script.")
else:
    arena = unreal.EditorLevelLibrary.spawn_actor_from_class(arena_cls, unreal.Vector(0, 0, 0))
    arena.set_actor_label("Pista")
    unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(-540, 0, 50))
    unreal.EditorLevelLibrary.save_current_level()
    log("Mapa /Game/Maps/Pista listo. Dale Play.")
