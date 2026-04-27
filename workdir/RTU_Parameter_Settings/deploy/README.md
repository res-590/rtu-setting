# 发布与在线升级

## 发布目录结构

打包后的目录建议保持如下结构：

```text
RTU_Parameter_Config_Tool/
  RTU_Parameter_Config_Tool.exe
  updater.exe
  dashboard_dark.qss
  version.txt
  baseparaminfo.ini
  instancemap.ini
  sensorinstance.ini
  sensortypeitemconfig.ini
  config/
    update_config.ini
  platforms/
  styles/
  iconengines/
  imageformats/
```

说明：

- `RTU_Parameter_Config_Tool.exe` 是主程序。
- `updater.exe` 是独立升级器，主程序在线升级时会复制它到临时目录执行。
- `version.txt` 用于记录当前安装版本。
- `*.ini` 视为用户配置，升级器默认保留现有同名 `ini`，不会强制覆盖。
- `config/update_config.ini` 存放在线升级配置。

## 在线升级流程

主程序在线升级按下面流程运行：

1. 程序启动后读取 `config/update_config.ini`。
2. 如果启用了自动检查，则请求 `ManifestUrl` 指向的升级清单 JSON。
3. 如果检测到新版本，则提示用户是否下载。
4. 主程序下载新的发布 zip，校验 `sha256`。
5. 主程序把 `updater.exe` 复制到临时目录并启动。
6. 主程序退出。
7. 升级器等待主程序进程结束后解压 zip，覆盖安装目录。
8. 升级器保留已有的 `.ini` 配置文件，更新完成后自动重启主程序。

## 升级清单格式

参考 [update_manifest.sample.json](./update_manifest.sample.json)：

```json
{
  "version": "1.0.1",
  "url": "https://your-server.example.com/rtu-config-tool/RTU_Parameter_Config_Tool-1.0.1.zip",
  "sha256": "replace_with_zip_sha256",
  "notes": "1. 修复通信设置日志展示"
}
```

字段要求：

- `version`: 目标版本号，建议用 `1.0.1` 这种格式。
- `url`: 可直接下载的 zip 地址。
- `sha256`: zip 包的 SHA-256，建议始终提供。
- `notes`: 更新说明，可选。

## 更新配置文件

参考 [update_config.ini.example](./update_config.ini.example)：

```ini
[Update]
Enabled=true
AutoCheckOnStartup=true
RequestTimeoutMs=15000
ManifestUrl=https://raw.githubusercontent.com/your-user/your-repo/main/update/update_manifest.json
UpdaterExecutable=updater.exe
```

程序会按下面顺序自动查找配置文件：

1. `程序目录/config/update_config.ini`
2. `程序目录/update_config.ini`
3. `当前工作目录/config/update_config.ini`
4. `当前工作目录/update_config.ini`
5. 上级目录中的同名配置
6. 上级目录中的 `deploy/update_config.ini.example`

这意味着开发时直接运行 `debug` 目录下的 exe，也能回退找到项目里的模板配置。

## GitHub 使用方式

推荐做法：

1. 把发布 zip 上传到 GitHub Releases
2. 把 `update_manifest.json` 放到仓库里，例如 `update/update_manifest.json`
3. `ManifestUrl` 使用 `raw.githubusercontent.com` 地址

示例：

- 清单地址  
  `https://raw.githubusercontent.com/res-590/rtu-setting/main/update/update_manifest.json`
- zip 下载地址  
  `https://github.com/res-590/rtu-setting/releases/download/v1.0.1/RTU_Parameter_Config_Tool-1.0.1.zip`

## GitHub 清单生成脚本

脚本：`deploy/generate_github_manifest.ps1`

示例：

```powershell
powershell -ExecutionPolicy Bypass -File .\deploy\generate_github_manifest.ps1 `
  -ZipPath .\publish\RTU_Parameter_Config_Tool-1.0.1.zip `
  -Version 1.0.1 `
  -Repo res-590/rtu-setting `
  -Notes "1. 修复日志显示`r`n2. 优化界面布局"
```

生成后把 `update_manifest.json` 提交到仓库的 `update/` 目录即可。

## 打包脚本

脚本：`deploy/package_release.ps1`

默认行为：

- 编译主程序
- 编译升级器
- 调用 `windeployqt`
- 收集 `exe`、Qt 依赖、`qss`、`ini`
- 生成 `publish/RTU_Parameter_Config_Tool`
- 生成发布 zip

示例：

```powershell
powershell -ExecutionPolicy Bypass -File .\deploy\package_release.ps1
```

如果已经手工编译完成，也可以跳过编译：

```powershell
powershell -ExecutionPolicy Bypass -File .\deploy\package_release.ps1 -SkipBuild
```
