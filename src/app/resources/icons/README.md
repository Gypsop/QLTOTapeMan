# QLTOTapeMan Icon Resources

This directory contains icon resources for the QLTOTapeMan application.

## Required Icons

The following icons are referenced in `resources.qrc`:

### Application Icons
- `app.ico` - Windows application icon (multi-size ICO file)
- `app.png` - Application icon (256x256 PNG)

### Toolbar Icons (24x24 PNG)
- `connect.png` - Connect to device
- `disconnect.png` - Disconnect from device
- `refresh.png` - Refresh device list
- `write.png` - Start write operation
- `read.png` - Read from tape
- `eject.png` - Eject tape
- `settings.png` - Open settings
- `about.png` - About dialog

### File Browser Icons (16x16 PNG)
- `folder.png` - Folder icon
- `file.png` - Generic file icon
- `drive.png` - Drive icon

### Status Icons (16x16 PNG)
- `status_ok.png` - OK/Ready status
- `status_warning.png` - Warning status
- `status_error.png` - Error status
- `status_busy.png` - Busy/Loading status

## Icon Guidelines

1. Use PNG format with transparency for best compatibility
2. Toolbar icons should be 24x24 pixels
3. Small icons (file browser, status) should be 16x16 pixels
4. App icon should be provided in multiple sizes (16, 32, 48, 256)
5. Maintain consistent visual style across all icons

## Creating app.ico

On Windows, use a tool like ImageMagick or IcoFX to create a multi-size ICO:

```bash
# Using ImageMagick
convert app.png -define icon:auto-resize=256,128,64,48,32,16 app.ico
```

## Placeholder Icons

For development, you can use placeholder icons. Replace them with proper icons
before release.

## License

Icons should be either:
- Created specifically for this project (owned by the project)
- From an open-source icon set with compatible license (MIT, Apache, etc.)
- From a public domain source

Document the source and license of any third-party icons used.
