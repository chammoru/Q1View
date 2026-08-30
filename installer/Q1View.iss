#define MyAppName "Q1View"
#define MyAppPublisher "Kyuwon Kim"
#define MyAppURL "https://github.com/chammoru/Q1View"

#ifndef AppVersion
#define AppVersion "0.0.0"
#endif

#ifndef SourceDir
#define SourceDir "..\dist\Q1View-windows-x64"
#endif

#ifndef OutputDir
#define OutputDir "..\dist"
#endif

#ifndef OutputBaseFilename
#define OutputBaseFilename "Q1ViewSetup-x64"
#endif

[Setup]
AppId={{70B9D29E-7CC0-40E4-A0A7-D83CC8F6C175}
AppName={#MyAppName}
AppVersion={#AppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}/issues
AppUpdatesURL={#MyAppURL}/releases
DefaultDirName={localappdata}\Programs\Q1View
DefaultGroupName=Q1View
DisableProgramGroupPage=yes
UninstallDisplayIcon={app}\Viewer.exe
OutputDir={#OutputDir}
OutputBaseFilename={#OutputBaseFilename}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=lowest
CloseApplications=yes
RestartApplications=no
SetupLogging=yes
LicenseFile=..\LICENSE
ChangesAssociations=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create desktop shortcuts"; GroupDescription: "Additional shortcuts:"; Flags: unchecked

[Files]
Source: "{#SourceDir}\Viewer.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\Comparator.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\*.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\CHANGELOG.md"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#SourceDir}\LICENSE"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#SourceDir}\README.md"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#SourceDir}\Icons\Q1ViewPhoto.ico"; DestDir: "{app}\Icons"; Flags: ignoreversion
Source: "{#SourceDir}\Icons\Q1ViewVideo.ico"; DestDir: "{app}\Icons"; Flags: ignoreversion
Source: "{#SourceDir}\Icons\Q1ViewRaw.ico"; DestDir: "{app}\Icons"; Flags: ignoreversion

[InstallDelete]
Type: files; Name: "{app}\libx265.dll"

[UninstallDelete]
Type: files; Name: "{app}\Viewer.exe"
Type: files; Name: "{app}\Comparator.exe"
Type: files; Name: "{app}\*.dll"
Type: files; Name: "{app}\CHANGELOG.md"
Type: files; Name: "{app}\LICENSE"
Type: files; Name: "{app}\README.md"
Type: files; Name: "{app}\Icons\Q1ViewPhoto.ico"
Type: files; Name: "{app}\Icons\Q1ViewVideo.ico"
Type: files; Name: "{app}\Icons\Q1ViewRaw.ico"
Type: dirifempty; Name: "{app}\Icons"
Type: dirifempty; Name: "{app}"

[Icons]
Name: "{group}\Q1View Viewer"; Filename: "{app}\Viewer.exe"; WorkingDir: "{app}"; IconFilename: "{app}\Viewer.exe"
Name: "{group}\Q1View Comparator"; Filename: "{app}\Comparator.exe"; WorkingDir: "{app}"; IconFilename: "{app}\Comparator.exe"
Name: "{autodesktop}\Q1View Viewer"; Filename: "{app}\Viewer.exe"; WorkingDir: "{app}"; IconFilename: "{app}\Viewer.exe"; Tasks: desktopicon
Name: "{autodesktop}\Q1View Comparator"; Filename: "{app}\Comparator.exe"; WorkingDir: "{app}"; IconFilename: "{app}\Comparator.exe"; Tasks: desktopicon

[Registry]
Root: HKA; Subkey: "Software\Classes\q1view.photo"; ValueType: string; ValueName: ""; ValueData: "Q1View Photo"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\q1view.photo\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\Icons\Q1ViewPhoto.ico,0"
Root: HKA; Subkey: "Software\Classes\q1view.photo\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\Viewer.exe"" ""%1"""
Root: HKA; Subkey: "Software\Classes\q1view.video"; ValueType: string; ValueName: ""; ValueData: "Q1View Video"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\q1view.video\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\Icons\Q1ViewVideo.ico,0"
Root: HKA; Subkey: "Software\Classes\q1view.video\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\Viewer.exe"" ""%1"""
Root: HKA; Subkey: "Software\Classes\q1view.raw"; ValueType: string; ValueName: ""; ValueData: "Q1View Raw Frame"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\q1view.raw\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\Icons\Q1ViewRaw.ico,0"
Root: HKA; Subkey: "Software\Classes\q1view.raw\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\Viewer.exe"" ""%1"""

; Keep the pre-2.7.13 ProgID functional during upgrades. Newly selected
; associations use the purpose-specific ProgIDs below.
Root: HKA; Subkey: "Software\Classes\q1viewfile"; ValueType: string; ValueName: ""; ValueData: "Q1View Supported File"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\q1viewfile\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\Icons\Q1ViewPhoto.ico,0"
Root: HKA; Subkey: "Software\Classes\q1viewfile\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\Viewer.exe"" ""%1"""

; Raw Formats
Root: HKA; Subkey: "Software\Classes\.yuv"; ValueType: string; ValueName: ""; ValueData: "q1view.raw"; Flags: createvalueifdoesntexist
Root: HKA; Subkey: "Software\Classes\.yuv\OpenWithProgids"; ValueType: string; ValueName: "q1view.raw"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\.rgb"; ValueType: string; ValueName: ""; ValueData: "q1view.raw"; Flags: createvalueifdoesntexist
Root: HKA; Subkey: "Software\Classes\.rgb\OpenWithProgids"; ValueType: string; ValueName: "q1view.raw"; ValueData: ""; Flags: uninsdeletevalue

; High-Efficiency Images
Root: HKA; Subkey: "Software\Classes\.heic"; ValueType: string; ValueName: ""; ValueData: "q1view.photo"; Flags: createvalueifdoesntexist
Root: HKA; Subkey: "Software\Classes\.heic\OpenWithProgids"; ValueType: string; ValueName: "q1view.photo"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\.heif"; ValueType: string; ValueName: ""; ValueData: "q1view.photo"; Flags: createvalueifdoesntexist
Root: HKA; Subkey: "Software\Classes\.heif\OpenWithProgids"; ValueType: string; ValueName: "q1view.photo"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\.hif"; ValueType: string; ValueName: ""; ValueData: "q1view.photo"; Flags: createvalueifdoesntexist
Root: HKA; Subkey: "Software\Classes\.hif\OpenWithProgids"; ValueType: string; ValueName: "q1view.photo"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\.avif"; ValueType: string; ValueName: ""; ValueData: "q1view.photo"; Flags: createvalueifdoesntexist
Root: HKA; Subkey: "Software\Classes\.avif\OpenWithProgids"; ValueType: string; ValueName: "q1view.photo"; ValueData: ""; Flags: uninsdeletevalue

; Standard Image Formats
Root: HKA; Subkey: "Software\Classes\.bmp"; ValueType: string; ValueName: ""; ValueData: "q1view.photo"; Flags: createvalueifdoesntexist
Root: HKA; Subkey: "Software\Classes\.bmp\OpenWithProgids"; ValueType: string; ValueName: "q1view.photo"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\.png"; ValueType: string; ValueName: ""; ValueData: "q1view.photo"; Flags: createvalueifdoesntexist
Root: HKA; Subkey: "Software\Classes\.png\OpenWithProgids"; ValueType: string; ValueName: "q1view.photo"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\.webp"; ValueType: string; ValueName: ""; ValueData: "q1view.photo"; Flags: createvalueifdoesntexist
Root: HKA; Subkey: "Software\Classes\.webp\OpenWithProgids"; ValueType: string; ValueName: "q1view.photo"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\.jpg"; ValueType: string; ValueName: ""; ValueData: "q1view.photo"; Flags: createvalueifdoesntexist
Root: HKA; Subkey: "Software\Classes\.jpg\OpenWithProgids"; ValueType: string; ValueName: "q1view.photo"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\.jpeg"; ValueType: string; ValueName: ""; ValueData: "q1view.photo"; Flags: createvalueifdoesntexist
Root: HKA; Subkey: "Software\Classes\.jpeg\OpenWithProgids"; ValueType: string; ValueName: "q1view.photo"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\.tif"; ValueType: string; ValueName: ""; ValueData: "q1view.photo"; Flags: createvalueifdoesntexist
Root: HKA; Subkey: "Software\Classes\.tif\OpenWithProgids"; ValueType: string; ValueName: "q1view.photo"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\.tiff"; ValueType: string; ValueName: ""; ValueData: "q1view.photo"; Flags: createvalueifdoesntexist
Root: HKA; Subkey: "Software\Classes\.tiff\OpenWithProgids"; ValueType: string; ValueName: "q1view.photo"; ValueData: ""; Flags: uninsdeletevalue

; Video Formats
Root: HKA; Subkey: "Software\Classes\.mp4"; ValueType: string; ValueName: ""; ValueData: "q1view.video"; Flags: createvalueifdoesntexist
Root: HKA; Subkey: "Software\Classes\.mp4\OpenWithProgids"; ValueType: string; ValueName: "q1view.video"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\.m4v"; ValueType: string; ValueName: ""; ValueData: "q1view.video"; Flags: createvalueifdoesntexist
Root: HKA; Subkey: "Software\Classes\.m4v\OpenWithProgids"; ValueType: string; ValueName: "q1view.video"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\.mov"; ValueType: string; ValueName: ""; ValueData: "q1view.video"; Flags: createvalueifdoesntexist
Root: HKA; Subkey: "Software\Classes\.mov\OpenWithProgids"; ValueType: string; ValueName: "q1view.video"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\.avi"; ValueType: string; ValueName: ""; ValueData: "q1view.video"; Flags: createvalueifdoesntexist
Root: HKA; Subkey: "Software\Classes\.avi\OpenWithProgids"; ValueType: string; ValueName: "q1view.video"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\.mkv"; ValueType: string; ValueName: ""; ValueData: "q1view.video"; Flags: createvalueifdoesntexist
Root: HKA; Subkey: "Software\Classes\.mkv\OpenWithProgids"; ValueType: string; ValueName: "q1view.video"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\.webm"; ValueType: string; ValueName: ""; ValueData: "q1view.video"; Flags: createvalueifdoesntexist
Root: HKA; Subkey: "Software\Classes\.webm\OpenWithProgids"; ValueType: string; ValueName: "q1view.video"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\.wmv"; ValueType: string; ValueName: ""; ValueData: "q1view.video"; Flags: createvalueifdoesntexist
Root: HKA; Subkey: "Software\Classes\.wmv\OpenWithProgids"; ValueType: string; ValueName: "q1view.video"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\.mpg"; ValueType: string; ValueName: ""; ValueData: "q1view.video"; Flags: createvalueifdoesntexist
Root: HKA; Subkey: "Software\Classes\.mpg\OpenWithProgids"; ValueType: string; ValueName: "q1view.video"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\.mpeg"; ValueType: string; ValueName: ""; ValueData: "q1view.video"; Flags: createvalueifdoesntexist
Root: HKA; Subkey: "Software\Classes\.mpeg\OpenWithProgids"; ValueType: string; ValueName: "q1view.video"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\.flv"; ValueType: string; ValueName: ""; ValueData: "q1view.video"; Flags: createvalueifdoesntexist
Root: HKA; Subkey: "Software\Classes\.flv\OpenWithProgids"; ValueType: string; ValueName: "q1view.video"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\.3gp"; ValueType: string; ValueName: ""; ValueData: "q1view.video"; Flags: createvalueifdoesntexist
Root: HKA; Subkey: "Software\Classes\.3gp\OpenWithProgids"; ValueType: string; ValueName: "q1view.video"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\.ts"; ValueType: string; ValueName: ""; ValueData: "q1view.video"; Flags: createvalueifdoesntexist
Root: HKA; Subkey: "Software\Classes\.ts\OpenWithProgids"; ValueType: string; ValueName: "q1view.video"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\.m2ts"; ValueType: string; ValueName: ""; ValueData: "q1view.video"; Flags: createvalueifdoesntexist
Root: HKA; Subkey: "Software\Classes\.m2ts\OpenWithProgids"; ValueType: string; ValueName: "q1view.video"; ValueData: ""; Flags: uninsdeletevalue

[Run]
Filename: "{app}\Viewer.exe"; Description: "Launch Q1View Viewer"; Flags: nowait postinstall skipifsilent
