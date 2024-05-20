package kgfw;

public class KGFW {
    static {
        try {
            System.loadLibrary("kgfwengine");
        } catch (Exception e) {
            e.printStackTrace();
            System.err.println("Failed to load KGFW lib");
            throw new RuntimeException("Failed to load KGFW lib");
        }
    }

    /* kgfw_input */
    public static native boolean inputGetKey(int key);
    public static native boolean inputGetKeyDown(int key);
    public static native boolean inputGetKeyUp(int key);
    public static native boolean inputGetMouseButton(int button);
    public static native double inputGetMouseDeltaX();
    public static native double inputGetMouseDeltaY();
    public static native double inputGetMouseScrollX();
    public static native double inputGetMouseScrollY();
    public static native double inputGetMousePosX();
    public static native double inputGetMousePosY();

    /* kgfw_log */
    public static native void logc(int severity, char character);
    public static native void log(int severity, String message);

    /* kgfw_ecs */
    public static native Entity newEntity(String name);
    public static native Entity copyEntity(String name, Entity sourceEntity);
    public static native Entity findEntity(String name);
    public static native Entity findEntity(long instanceID);
    public static native Component entityGetComponent(Entity entity, long componentID);
    public static native Component entityAttachComponent(Entity entity, String componentName);
    public static native Component entityAttachComponent(Entity entity, long componentID);
    public static native void destroyComponent(Component component);
}
