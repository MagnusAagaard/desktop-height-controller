# SPIFFS Web Files Directory

This directory contains static files that will be uploaded to the ESP32's SPIFFS partition.

## Contents

- `index.html` - Main web interface
- `style.css` - Responsive stylesheets
- `script.js` - JavaScript for SSE client and form handling

## Uploading to ESP32

Use PlatformIO to upload the filesystem:

```bash
pio run --target uploadfs
```

## Size Constraints

- Total SPIFFS partition: ~200KB
- Keep total file size under 180KB to leave room for wear leveling
