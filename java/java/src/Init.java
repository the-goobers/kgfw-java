import kgfw.Component;
import kgfw.Entity;
import kgfw.KGFW;

public class Init {
    private static Entity entity = null;
    private static Component component = null;

    public static boolean init() {
        KGFW.log(0, "init java");
        entity = KGFW.newEntity("Bob");
        component = KGFW.entityAttachComponent(entity, "scripts/game/Init");
        KGFW.log(0, "init java success");
        return true;
    }

    public static void deinit() {
        System.out.println("deinit");
        KGFW.log(0, "deinit java " + entity);
        KGFW.destroyEntity(entity);
    }
}
