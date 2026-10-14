"""Recupera las texturas que se perdieron cuando setup_level.py borró y volvió a crear M_Color / M_Cutout.
Los materiales tuyos que heredaban de ellos quedaron "sin padre" (por eso se ven grises). Este script los vuelve a
enganchar: sus texturas y colores siguen guardados adentro, así que vuelven a verse como antes.
Se corre en el editor: Tools > Execute Python Script... > Tools/recuperar_texturas.py"""
import unreal

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
reg = unreal.AssetRegistryHelpers.get_asset_registry()


def log(m):
    unreal.log("[PenalPatines] " + m)


m_color = eal.load_asset("/Game/Materials/M_Color")
m_cut = eal.load_asset("/Game/Materials/M_Cutout")
fixed = 0; total = 0
for d in reg.get_assets_by_path("/Game", recursive=True):
    if str(d.asset_class_path.asset_name) != "MaterialInstanceConstant":
        continue
    total += 1
    mi = d.get_asset()
    if mi is None or mi.get_editor_property("parent") is not None:
        continue
    name = str(d.asset_name).lower()
    cut = any(k in name for k in ("cutout", "hair", "pelo", "brow", "lash", "recorte", "alpha", "hoja", "leaf", "arbol", "tree"))
    parent = m_cut if (cut and m_cut) else m_color
    if parent is None:
        log("No hay M_Color para enganchar " + str(d.package_name)); continue
    mel.set_material_instance_parent(mi, parent)
    eal.save_loaded_asset(mi)
    fixed += 1
    log("Recuperado: %s -> %s" % (d.package_name, parent.get_name()))
log("Instancias de material revisadas: %d, recuperadas: %d" % (total, fixed))
if fixed == 0:
    log("No había materiales sin padre. Si siguen grises, las texturas se perdieron de otra forma: mandame estas líneas.")
