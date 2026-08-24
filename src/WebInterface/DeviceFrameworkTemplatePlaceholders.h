#ifndef DEVICEFRAMEWORK_TEMPLATE_PLACEHOLDERS_H
#define DEVICEFRAMEWORK_TEMPLATE_PLACEHOLDERS_H

#ifdef ENABLE_WEB_INTERFACE
#include <TemplateEngine.h>

/**
 * DeviceFramework-specific placeholder management
 * Owns and manages the placeholder registry for the web interface
 */
class DeviceFrameworkTemplatePlaceholders {
public:
    /**
     * Initialize the placeholder system
     * Creates registry and registers all placeholders
     * Call this once during web interface setup
     */
    static void setup();

    /**
     * Cleanup the placeholder system
     * Clears and destroys the registry
     * Call during web interface shutdown
     */
    static void cleanup();

    /**
     * Get the placeholder registry
     * @return Pointer to registry, or nullptr if not initialized
     */
    static PlaceholderRegistry* getRegistry();

    /**
     * Check if placeholders are initialized
     */
    static bool isInitialized() { return registry != nullptr; }

private:
    // Registry instance (owned by this class)
    static PlaceholderRegistry* registry;
    static bool isSetup;

    // Getter functions for dynamic data
    static const char* getPageTitle();
    static const char* getPageTitle404();

    // Internal method to register all placeholders
    static void registerAllPlaceholders();
};

#endif // ENABLE_WEB_INTERFACE
#endif // DEVICEFRAMEWORK_TEMPLATE_PLACEHOLDERS_H
