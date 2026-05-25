# Security Policy

## Supported Versions

Security fixes are applied to the latest release only.

| Version | Supported |
| --- | --- |
| Latest release | Yes |
| Older releases | No |

## Reporting a Vulnerability

Please **do not** open a public GitHub issue for security vulnerabilities.

Report vulnerabilities privately by emailing **chammoru@gmail.com** with:

- A description of the vulnerability and its potential impact.
- Steps to reproduce or a proof-of-concept if available.
- Any suggested mitigations you have identified.

You can expect an acknowledgement within a few business days. Once the issue is
confirmed, a fix will be prepared and a patched release published before any
public disclosure.

## Scope

Q1View is a local Windows desktop application. It reads image, video, and raw
frame files from the local filesystem and does not make network connections
during normal use. The primary attack surface is malformed input files passed to
the image/video decoders (OpenCV/FFmpeg and libheif).
