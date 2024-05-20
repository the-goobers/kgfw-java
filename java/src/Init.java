import kgfw.Component;
import kgfw.Entity;
import kgfw.KGFW;

public class Init {
    public static boolean init() {
        KGFW.log(0, "init java");
        Entity e = KGFW.newEntity("Bob");
        Component c = KGFW.entityAttachComponent(e, "scripts/game/Test");
        KGFW.log(0, "init java success");
        return true;
    }

    public static void deinit() {
        System.out.println("deinit");
        KGFW.log(0, "deinit java");
    }
}
