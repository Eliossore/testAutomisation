#define MyAppName "Ecofloc"
#define MyAppVersion "0.85"
#define MyAppPublisher "Ecofloc4win team"
#define Installer "Ecofloc4win team"
[Setup]
;App Info
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\ecofloc

;Setup Style and Images
WizardStyle=modern
UninstallDisplayIcon=images\ecoflocV2.ico
SetupIconFile=images\ecoflocV2.ico
DisableWelcomePage=no
WizardImageFile=images\ecoflocV2.bmp

;Installer name
OutputBaseFilename=ecofloc-installer

;Files for the user to read and accept
LicenseFile=LICENSE
InfoBeforeFile=README.md

;Type of compression for the installer
Compression=lzma
SolidCompression=yes

;¯\_(ツ)_/¯
Output=yes

;Different language files for the installer, for now it's the compiler default, might add more later
[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "vite-server.exe"; DestDir: "{app}"; Flags: ignoreversion; Components: ui
Source: "ecofloc-UI\*"; DestDir: "{app}\ecofloc-UI\"; Flags: ignoreversion recursesubdirs createallsubdirs ; Components: ui
Source: "ecofloc-Win\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: main

[Components]
Name: "main"; Description: "Ecofloc"; Types: full compact custom; Flags: fixed
Name: "ui"; Description: "Web UI"; Types: full

;Adds the app to the Path (bit of a pain to remove tho)
[Registry]
Root: HKLM; Subkey: "SYSTEM\CurrentControlSet\Control\Session Manager\Environment"; ValueType: expandsz; ValueName: "Path"; ValueData: "{olddata};{app}";Components: main

[Code]

const
  EnvironmentKey = 'SYSTEM\CurrentControlSet\Control\Session Manager\Environment'; // To get the path to the... well Path
  RequiredDotNetVersion = '8.0.10'; // Version of .NET Desktop Runtime Required to execute EcoflocConfigurator.exe
  // Below all the possible url for each versions of a CPU
  DotNetInstallerURL64 = 'https://download.visualstudio.microsoft.com/download/pr/f398d462-9d4e-4b9c-abd3-86c54262869a/4a8e3a10ca0a9903a989578140ef0499/windowsdesktop-runtime-8.0.10-win-x64.exe'; // Replace with the actual URL
  DotNetInstallerURL86 = 'https://download.visualstudio.microsoft.com/download/pr/9836a475-66af-47eb-a726-8046c47ce6d5/ccb7d60db407a6d022a856852ef9e763/windowsdesktop-runtime-8.0.10-win-x86.exe';
  DotNetInstallerURLArm64 = 'https://download.visualstudio.microsoft.com/download/pr/c1387fab-1960-4cdc-8653-1e0333f6385a/3bd819d5f2aecff94803006a9e2c945a/windowsdesktop-runtime-8.0.10-win-arm64.exe';

function ProcArchi(): String;
begin
  case ProcessorArchitecture of
    paX86: Result := 'x86';
    paX64: Result := 'x64';
    paArm64: Result := 'arm64';
  else
    Result := 'Unrecognized';
  end;
end;


// No idea what it is it is necessary to use DownloadTemporaryFile tho
function OnDownloadProgress(const Url, Filename: String; const Progress, ProgressMax: Int64): Boolean;
begin
  if ProgressMax <> 0 then
    Log(Format('  %d of %d bytes done.', [Progress, ProgressMax]))
  else
    Log(Format('  %d bytes done.', [Progress]));
  Result := True;
end;

// Check if the correct version of .NET is installed
function IsDotNetDesktopRuntimeVersionInstalled(Version: String): Boolean;
var
  RuntimePath: String;
begin
  RuntimePath := ExpandConstant('{pf}\dotnet\shared\Microsoft.WindowsDesktop.App\' + Version);
  Result := DirExists(RuntimePath);
end;

// Download and Install .NET Desktop Runtime
function DownloadAndInstallDotNet: Boolean;
var
  DownloadURL: string;
  DotNetInstallerPath: string;
  TempFilePath: string;
  ExecResult: Integer;
  Proc: String;
  ResultCode: Integer;
begin
  Result := False;
  Proc := ProcArchi;
  case ProcessorArchitecture of
    paX86: DownloadURL := DotNetInstallerURL86;
    paX64: DownloadURL := DotNetInstallerURL64;
    paArm64: DownloadURL := DotNetInstallerURLArm64;
  else
    Result := False;
  end;
  if not IsDotNetDesktopRuntimeVersionInstalled('8.0.10') then
  begin
    DownloadTemporaryFile(DownloadURL, 'desktop-runtime.exe', '', @OnDownloadProgress);
    DotNetInstallerPath := ExpandConstant('{tmp}\desktop-runtime.exe');
    MsgBox(
      'The .NET Desktop Runtime version 8.0.10 is required to run this application. ' +
      'We are now going to download and prompt you to install it',
      mbError,
      MB_OK
    );
    Result := False;
    if Exec(DotNetInstallerPath, '', '', SW_SHOWNORMAL, ewWaitUntilTerminated, ResultCode) then
    begin
      MsgBox('Executable ran successfully. Exit code: ' + IntToStr(ResultCode), mbInformation, MB_OK);
    end
    else
    begin
      MsgBox('Error executing the file. Exit code: ' + IntToStr(ResultCode), mbError, MB_OK);
    end;
  end;
end;


// The function that removes Ecofloc from the Path
procedure RemovePath(Path: string);
var
  Paths: string;
  P: Integer;
begin
  if not RegQueryStringValue(HKLM, EnvironmentKey, 'Path', Paths) then
  begin
    Log('PATH not found');
  end
    else
  begin
    Log(Format('PATH is [%s]', [Paths]));

    P := Pos(';' + Uppercase(Path) + ';', ';' + Uppercase(Paths) + ';');
    if P = 0 then
    begin
      Log(Format('Path [%s] not found in PATH', [Path]));
    end
      else
    begin
      if P > 1 then P := P - 1;
      Delete(Paths, P, Length(Path) + 1);
      Log(Format('Path [%s] removed from PATH => [%s]', [Path, Paths]));

      if RegWriteStringValue(HKLM, EnvironmentKey, 'Path', Paths) then
      begin
        Log('PATH written');
      end
        else
      begin
        Log('Error writing PATH');
      end;
    end;
  end;
end;

// Changes the steps of the uninstall process to be able to execute custom code
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usUninstall then
  begin
    RemovePath(ExpandConstant('{app}'));
  end;
end;
function IsNodeInstalled(): Boolean;
var
  ErrorCode: Integer;
begin
  Result := False;
  Exec('cmd.exe', '/C node -v', '', SW_HIDE, ewWaitUntilTerminated, ErrorCode);
  if ErrorCode = 0 then
    Result := True;
end;

//Check if npm is installed
function IsNpmInstalled(): Boolean;
var
  ErrorCode: Integer;
begin
  Result := False;
  Exec('cmd.exe', '/C npm -v', '', SW_HIDE, ewWaitUntilTerminated, ErrorCode);
  if ErrorCode = 0 then
    Result := True;
end;

//Check if node is installed
procedure InstallNode();
var
  ResultCode: Integer;
begin
  MsgBox('Node is not installed. Installation of Node will now start', mbInformation, MB_OK);
  if not Exec('msiexec.exe', '/i https://nodejs.org/dist/v18.17.1/node-v18.17.1-x64.msi /quiet /norestart', '', SW_HIDE, ewWaitUntilTerminated, ResultCode) then
  begin
    MsgBox('Unable to Node. Try again or install it manually', mbError, MB_OK);
  end;
end;

// changes the step for the installation process to executes custom code
procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
  begin
    DownloadAndInstallDotNet();
    if IsComponentSelected('ui') then
    begin
      if not IsNodeInstalled() or not IsNpmInstalled() then
      begin
        InstallNode();
      end
      else
      begin
        MsgBox('Node is already installed on your system', mbInformation, MB_OK);
      end;
    end;
  end;
end;