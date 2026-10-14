"""Deshace lo que agregó por error setup_level.py a tu mapa (una segunda pista con parquet, hinchada y luz de noche)
y deja tu estadio como estaba. Abrí TU mapa en el editor y corré:  Tools > Execute Python Script... > Tools/arreglar_mapa.py
No borra nada tuyo: solo la pista y el PlayerStart que agregó el script (los que tienen los valores de fábrica)."""
import unreal

ell = unreal.EditorLevelLibrary
eal = unreal.EditorAssetLibrary


def log(m):
    unreal.log("[PenalPatines] " + m)


actors = ell.get_all_level_actors()
arena_cls = getattr(unreal, "PPArena", None)
arenas = [a for a in actors if arena_cls and isinstance(a, arena_cls)]
log("Pistas (PPArena) en el mapa: %d" % len(arenas))


def is_factory(a):
    try:
        return a.get_editor_property("crowd_density") == 1 and a.get_editor_property("floor_material") is None
    except Exception:
        return False


for a in arenas:
    log("  %s  hinchada=%s  piso=%s" % (a.get_actor_label(), a.get_editor_property("crowd_density"), a.get_editor_property("floor_material")))

mine = [a for a in arenas if not is_factory(a)]
added = [a for a in arenas if is_factory(a)]
if mine and added:
    # tu pista tiene sus propios ajustes: la que agregó el script sobra
    for a in added:
        log("Borro la pista agregada: " + a.get_actor_label())
        ell.destroy_actor(a)
elif len(added) >= 1:
    # una sola pista con los valores de fábrica: le saco la hinchada, el parquet y la luz de noche
    keep = added[0]
    for a in added[1:]:
        log("Borro pista repetida: " + a.get_actor_label()); ell.destroy_actor(a)
    keep.set_editor_property("crowd_density", 0)
    keep.set_editor_property("spawn_post_process", False)
    # material de baldosas: el primero cuyo nombre lo indique
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    tile = None
    for d in reg.get_assets_by_path("/Game", recursive=True):
        n = str(d.asset_name).lower()
        cls = str(d.asset_class_path.asset_name)
        if cls in ("Material", "MaterialInstanceConstant") and any(k in n for k in ("baldosa", "tile", "ceramic", "piso", "floor")) and "parquet" not in n:
            tile = d.get_asset(); break
    if tile:
        keep.set_editor_property("floor_material", tile)
        log("Piso: " + tile.get_name())
    else:
        log("No encontré un material de baldosas: elegilo a mano en Details > Look > Floor Material")
    log("Pista sin hinchada y sin la luz de noche")

# PlayerStart repetido que agregó el script en (-540, 0, 50)
starts = [a for a in ell.get_all_level_actors() if isinstance(a, unreal.PlayerStart)]
if len(starts) > 1:
    for s in starts:
        p = s.get_actor_location()
        if abs(p.x + 540) < 1 and abs(p.y) < 1 and abs(p.z - 50) < 1 and len(starts) > 1:
            log("Borro PlayerStart agregado"); ell.destroy_actor(s); starts.remove(s); break

ell.save_current_level()
log("Listo: mapa guardado.")
