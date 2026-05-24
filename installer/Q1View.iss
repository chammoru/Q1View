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

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create desktop shortcuts"; GroupDescription: "Additional shortcuts:"; Flags: unchecked

[Files]
Source: "{#SourceDir}\Viewer.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\Comparer.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\*.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\CHANGELOG.md"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#SourceDir}\LICENSE"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#SourceDir}\README.md"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist

[InstallDelete]
Type: files; Name: "{app}\libx265.dll"

[UninstallDelete]
Type: files; Name: "{app}\Viewer.exe"
Type: files; Name: "{app}\Comparer.exe"
Type: files; Name: "{app}\*.dll"
Type: files; Name: "{app}\CHANGELOG.md"
Type: files; Name: "{app}\LICENSE"
Type: files; Name: "{app}\README.md"
Type: dirifempty; Name: "{app}"

[Icons]
Name: "{group}\Q1View Viewer"; Filename: "{app}\Viewer.exe"; WorkingDir: "{app}"; IconFilename: "{app}\Viewer.exe"
Name: "{group}\Q1View Comparer"; Filename: "{app}\Comparer.exe"; WorkingDir: "{app}"; IconFilename: "{app}\Comparer.exe"
Name: "{autodesktop}\Q1View Viewer"; Filename: "{app}\Viewer.exe"; WorkingDir: "{app}"; IconFilename: "{app}\Viewer.exe"; Tasks: desktopicon
Name: "{autodesktop}\Q1View Comparer"; Filename: "{app}\Comparer.exe"; WorkingDir: "{app}"; IconFilename: "{app}\Comparer.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\Viewer.exe"; Description: "Launch Q1View Viewer"; Flags: nowait postinstall skipifsilent
