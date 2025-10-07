(Translated into English by Google Translate)
# 如何制作捆绑包（AppxBundle）？<br>(How to create an AppxBundle?)
对于 MakeAppx 的使用完全可以参考这篇文章：https://learn.microsoft.com/windows/win32/appxpkg/make-appx-package--makeappx-exe-?redirectedfrom=MSDN<br>For the use of MakeAppx, you can refer to this article: https://learn.microsoft.com/windows/win32/appxpkg/make-appx-package--makeappx-exe-?redirectedfrom=MSDN

其实，可以完全用官方的 MakeAppx.exe （要求使用 Windows 8.1 及以上版本的 Kits）来制作。但是由于相较于打包成 Appx 包，打包成 AppxBundle 有点更麻烦。但还好，Windows 8.1 samples 是有使用 AppxPackaging.h 来打包捆绑包的例程。再结合之前 WSAppBak 的源码（项目：https://github.com/Wapitiii/WSAppBak）提供了如何对包签名的例程，所以就能做出一个打包成捆绑包的工具。<br>Actually, it's possible to create it entirely using the official MakeAppx.exe (requires Windows 8.1 Kits and above). However, packaging an AppxBundle is a bit more involved than packaging an Appx package. Fortunately, the Windows 8.1 samples include examples for packaging bundles using AppxPackaging.h. Combined with the WSAppBak source code (project: https://github.com/Wapitiii/WSAppBak), which provides examples for signing packages, a tool for packaging bundles can be created.

<h3>打包成捆绑包的要求：<br>Requirements for packaging into a bundle:</h3>
<ul>
  <li>对于 Windows 8 的应用（不是 8.1 的），可能无法打包成 AppxBundle。<br>For Windows 8 applications (not 8.1), it may not be possible to package them into AppxBundle.</li>
  <li>保持需要打包成捆绑包所用到的所有包的清单的大部分内容都要一致。<br>Keep most of the manifests of all packages that need to be packaged into a bundle consistent.</li>
  <li>捆绑包的版本号与欲打包成捆绑包的所用包的版本号并不一定是一致的。<br>The version number of the bundle is not necessarily the same as the version number of the packages used to make the bundle.</li>
</ul>

<h2>保持清单的一致：<br>Keep your list consistent:</h2>

Metro 应用在通过 Visual Studio 打包成 AppxBundle 时，所用清单都来自一个，所有的子包除了一些由于应用包和资源包的区别有些内容没有外，以及资源或架构的差异，剩下的所有资源的值都是一样的。所有应用包的清单（AppxManifest.xml）的之间仅有支持的架构的区别。而对于资源包，虽然相较于应用包缺失了许多信息（因为这些信息打包了没必要，本身一些内容也没有打包到资源包），但是除了架构（资源包无架构）和其余一些资源包独有的信息外，其余信息皆与应用包一致。修改资源的情况极少，目前对包的修复集中在应用包中对某些 API 的替换，所以大多数不用管资源包。而且资源包多且零散，修改可是个大问题。<br>When Metro apps are packaged as AppxBundles using Visual Studio, the manifest is sourced from a single source. All subpackages share the same resource values, with the exception of some missing content due to differences between app and resource bundles, as well as differences in resources or architecture. The manifests (AppxManifest.xml) of all app bundles differ only in supported architectures. Resource bundles, while lacking much information compared to app bundles (because it's unnecessary to package them, and some of their content isn't included), retain the same information as app bundles, aside from architecture (resource bundles don't have architectures) and some other information unique to resource bundles. Modifying resources is rare, as current package fixes focus on replacing certain APIs within app bundles, so resource bundles are generally not a concern. Furthermore, with so many and fragmented resource bundles, modifying them can be a significant challenge.

这里拿 migbrunluz2 的 New Retiled 项目中提供的 AccuWeather 举例。<br>Here's an example of AccuWeather provided by migbrunluz2's New Retiled project.
 
<img width="1450" height="777" alt="Packages From https://retiled.marmak.net.pl/releases/Apptravaganza%21/AccuWeather/" src="https://github.com/user-attachments/assets/f06de6ba-7c92-406c-8432-7bd20cda4c53" />

这里我通过我写的 应用安装程序（App Installer） 获取到的包的信息：<br>Here is the package information I obtained through the App Installer I wrote:

<img width="1920" height="1080" alt="Windows 8 1 x64-2025-10-07-16-30-32" src="https://github.com/user-attachments/assets/8568c19a-a62e-4bb0-916b-2eb36b314742" />

这里我标注了包的版本都是一致的，和相应适应的处理器架构。这里我特意标注了 x86 包的 StoreLogo 后的背景色。App Installer 在读取包时会读取清单中第一个 Application 的 VisualElement 中的 BackgroundColor 属性。（截图中标注 2 的部分是错误的）如果值为 transparent 才会显示系统用的主题色，否则一定会显示相应颜色。<br>I've noted that the package versions are consistent, along with the corresponding processor architectures. I've specifically noted the background color after the StoreLogo in the x86 package. When loading a package, the App Installer reads the BackgroundColor property of the first Application's VisualElement in the manifest. (The 2 marked in the screenshot is incorrect.) If the value is transparent, the system theme color will be displayed; otherwise, the corresponding color will be displayed.

<img width="1280" height="565" alt="屏幕截图 2025-10-07 180915" src="https://github.com/user-attachments/assets/504ba711-e430-4b6c-946d-47154c13dd4a" />

然后我们放入我编写的 Package To Bundle 中，准备打包成一个 AppxBundle 包。<br>Then we put it into the Package To Bundle I wrote and prepare to package it into an AppxBundle package.

<img width="1344" height="858" alt="Windows 8 1 x64-2025-10-07-16-33-32" src="https://github.com/user-attachments/assets/1ddce91e-09c5-4cef-aa54-870879c84c3c" />

但是打包过程中发现：失败了。错误信息显示是 x86 的包无法加入，这导致了打包失败。<br>However, during the packaging process, I found that it failed. The error message showed that the x86 package could not be added, which led to the packaging failure.

<img width="1332" height="858" alt="Windows 8 1 x64-2025-10-07-16-33-41" src="https://github.com/user-attachments/assets/df034c30-d432-4273-9dff-b921960339cc" />

之后我尝试将 x86 的包从列表中移除后，在试着打包，结果可以看到打包成功。<br>After that, I tried to remove the x86 package from the list and tried to pack it again. The result showed that the packaging was successful.

<img width="1704" height="984" alt="Windows 8 1 x64-2025-10-07-16-34-22" src="https://github.com/user-attachments/assets/1d465a8e-f381-4cf9-9d2d-b92ac97a2f3b" />

所以一定是出了某种问题，导致 x86 的包无法被打包进去。我对比了 arm 包和 x86 包的应用清单信息。结果可以看到，除了架构外，还有两处的值不一样：Splash Screen 时的背景色和 VisualElement 项中的背景色。<br>So there must be some problem preventing the x86 package from being included. I compared the app manifests of the ARM and x86 packages. Besides the architecture, I saw two other differences: the background color of the Splash Screen and the background color of the VisualElement item.

<img width="1920" height="1080" alt="Windows 8 1 x64-2025-10-07-16-38-14" src="https://github.com/user-attachments/assets/84dc3d4e-1bf9-4dbd-8ab8-05c964442999" />

于是我解压这个包后修改了应用清单，然后再通过工具（如 WSAppBak 等）重新打包成 Appx 包。然后我再将这个修改后的 x86 包添加到列表，结果发现打包成功。<br>So I unzipped the package, modified the app manifest, and then repackaged it into an Appx package using a tool (such as WSAppBak). I then added the modified x86 package to the manifest, and found that the package was successfully packaged.

<img width="1362" height="840" alt="Windows 8 1 x64-2025-10-07-16-45-12" src="https://github.com/user-attachments/assets/f7850f6a-f060-4bea-8e91-ef0ba1a99d29" />

（x86 包很明显我把背景色改了。）<br>(The x86 package obviously has the background color changed.)

<img width="1722" height="1080" alt="Windows 8 1 x64-2025-10-07-16-45-26" src="https://github.com/user-attachments/assets/b91a51e4-7ed0-4751-a8c7-5bb13fa50c72" />

可以看到，我新生成的包是支持 x86 的。<br>As you can see, my newly generated package supports x86.

<img width="888" height="612" alt="Windows 8 1 x64-2025-10-07-16-48-50" src="https://github.com/user-attachments/assets/27170f5b-0b70-4c3a-b070-f21652bca03f" />

我没验证过文件的一致性，我不清楚多个文件和少个文件是否影响打包，这个还是自己实验吧。当然，我们也只会修改文件而不会增删文件，因为我们也不知道会不会引起应用的 bug。<br>I haven't verified the consistency of the files, so I'm not sure if having more or less files affects the packaging. This is something you'll have to experiment with yourself. Of course, we'll only modify files and not add or delete them, as we don't know if this will cause bugs in the application.

<h2>对于 PRI 资源的说明：<br>Description of PRI resources:</h2>

PRI 文件通常位于 Appx 包中，与清单同一目录，且命名为“resources.pri”。PRI 资源在 AppxBundle 包没有出现前保存着所有的字符串资源和一些文件路径（如缩放资源，通常会见到 scale-80、100、140、180 的资源，指的就是这个）。这里只讲字符串资源。<br>The PRI file is typically located in the Appx package, in the same directory as the manifest, and is named "resources.pri." Before the AppxBundle package appears, the PRI resource stores all string resources and some file paths (such as scale resources, which you'll often see listed as scale-80, 100, 140, and 180). This article will focus on string resources.

一些应用有本地化的需求。对于 Windows 的程序，微软提供了一个资源管理的方法（RC）。这个方法将一些资源都编译到二进制中，如今我们也可以通过 Resource Hacker 就能查看到：<br>Some applications require localization. For Windows programs, Microsoft provides a resource manager (RC) that compiles some resources into the binary. These resources can now be viewed using Resource Hacker:

<img width="987" height="540" alt="屏幕截图 2025-10-07 183646" src="https://github.com/user-attachments/assets/8dad924a-132a-43a7-bbcb-1e4a7e98a902" />

而之后，则结合 mui 使用。因为让多个语言都集中到一处则显得很繁重，而分散来储存则就更加方便（也有点更难维护）<br>After that, it is used in conjunction with mui. Because it is very cumbersome to store multiple languages ​​in one place, it is more convenient to store them in a decentralized manner (but also a bit more difficult to maintain).

在 Metro 应用中，PRI 最早也是按照所有资源集中到一处来设置的。但是后面发现太繁重，之后就分成多个 PRI 文件。每个 PRI 文件存储一个资源。结合 AppxBundle 包，系统会根据当前的语言和 DPI 还有 DirectX 功能等级来安装相应的包，也是有效减少了资源的浪费。同时 AppxBundle 包也比之前单打包成 Appx 包大了点。<br>In Metro apps, the PRI was initially set up to centrally store all resources. However, this became too cumbersome, and the files were subsequently split into multiple PRI files. Each PRI file stores a single resource. Combined with AppxBundle packages, the system installs the appropriate package based on the current language, DPI, and DirectX feature level, effectively reducing resource waste. AppxBundle packages are also slightly larger than previously packaged Appx packages.

分成许多 PRI 文件后，会有一个主要的 PRI 文件位于应用包，而其余 PRI 文件都通过资源包来储存。<br>After being divided into many PRI files, there will be a main PRI file located in the application package, and the remaining PRI files are stored in the resource package.

而微软在 SDK 提供了 MakePRI 工具，可以创建或解析 PRI 文件。我编写的 应用安装程序 正是用到了这个工具帮助我从 PRI 文件获取到资源。<br>Microsoft provides the MakePRI tool in its SDK, which can create or parse PRI files. The application installer I wrote uses this tool to help me get resources from PRI files.

形容一下 PRI 文件的储存（我描述的并不一定准确，更多的是猜想，因为我是通过简单观察得出的）<br>Describe the storage of PRI files (my description may not be accurate, it is more of a guess because I made it based on simple observation)

要知道，当应用从 PRI 文件获取资源时，像是上学时，学哪些科目找某个科目的老师，而不会在意这个科目的老师具体是谁。至于具体是哪位老师，则是内部的安排。而内部会根据一些方法设置了负责这个科目的老师的安排。<br>It's important to understand that when an application retrieves resources from a PRI file, it's like looking for a specific teacher for a specific subject in school, regardless of the specific teacher. The specific teacher is assigned internally, and the teacher assignment for a specific subject is set internally using some method.

<img width="710" height="539" alt="Normal Condition" src="https://github.com/user-attachments/assets/50308af3-a42a-4817-9501-d314b9b804ca" />

但是谁也不知道两个相邻版本发布时语言资源到底修改了什么，鬼才知道修改了什么，有可能时增添，也有可能是删除。如果不是来自同一版本的 PRI 文件（PRI 文件本身没有版本信息，但是 PRI 文件所在的包却是有版本信息）。这就导致一个问题：位置对不上。<br>However, no one knows what language resources have been modified between two consecutive releases. It's possible additions or deletions. If the PRI files aren't from the same release (the PRI files themselves don't have version information, but the packages they're in do), this leads to a problem: misaligned locations.

<img width="710" height="539" alt="Abnormal Condition" src="https://github.com/user-attachments/assets/cf4c41a6-cca0-44d8-9400-8840e280de2e" />

这个“错位”的情况很少见，通常发生在，如你的应用本身就要使用应用包没有的资源（如我这个中文用户安装 MSN 应用时，除了会安装 x64 的包，还会安装 zh-Hans 语言包）。然后你获取到新版本的捆绑包，解压后单独安装了应用包，而没安装资源包，这就可能导致显示错误的情况。还有就是自己修改包，把资源修改的不符合要求。<br>This misalignment is rare and typically occurs when your app requires resources not included in the app bundle (for example, when I, a Chinese user, install the MSN app, I install both the x64 and zh-Hans language packs). Then, you obtain a new version of the bundle, unzip it, and install the app bundle separately without the resource bundle, which can cause display errors. Another possible cause is when you modify the bundle yourself, changing the resources to something that doesn't meet the requirements.

<img width="606" height="369" alt="无标题" src="https://github.com/user-attachments/assets/4f20a3fb-1fb5-4732-bb92-fc91c17de510" />

<h2>在这里说明一下 AppxBundle 包版本的问题：<br>Here is an explanation of the AppxBundle package version issue:</h2>

版本号的结构构成：&lt;主版本号 (Major)&gt;.&lt;次版本号 (Minor)&gt;.&lt;构建号 (Build)&gt;.&lt;修订号 (Revision，这样翻译吧？)&gt;。<br>The structure of the version number is: &lt;Major&gt;.&lt;Minor&gt;.&lt;Build&gt;.&lt;Revision&gt;.

虽然我的 应用安装程序 是为了方便看包的版本，并没显示 AppxBundle 的版本，而是 AppxBundle 包中 Appx 的版本，方便看版本号。但实际上 AppxBundle 包的版本号与包内的 Appx 包的版本号却不是一样的。在 Windows 8.1 应用中，主版本号通常为构建时的年份（如 2014、2015 等），而在 Windows 10 中，不会以年份为主版本号，好像有一段时期又以年份为主版本号。现在不知道变成什么样子了。但在这里，我们只需知道：Win8.1 的 AppxBundle 的版本号是以年份为主版本号的。<br>Although my App Installer doesn't display the AppxBundle version, but rather the Appx version within the AppxBundle, for easier viewing, the AppxBundle version number is actually different from the Appx version number within it. In Windows 8.1, the major version number for apps was typically the year of the build (e.g., 2014, 2015, etc.). In Windows 10, this no longer uses the year as the primary version number, though it seems that for a period of time, it was used again. I'm not sure what's changed now. For now, all we need to know is that AppxBundle versions in Windows 8.1 are based on the year.

<img width="633" height="627" alt="屏幕截图 2025-10-07 195618" src="https://github.com/user-attachments/assets/b68d2e38-7a96-4109-8200-17f82c82f70e" />

而对应用进行更新时：<br>When updating an app:
<ul>
  <li>对于 Appx 包：只会与当前计算机上安装的 Appx 包的版本号进行比对。这也就是为什么从 AppxBundle 解压后的 Appx 也能正常安装上。<br>For Appx packages: The version number is only compared with the Appx package currently installed on the computer. This is why Appx extracted from AppxBundle can also be installed normally.</li>
  <li>但对于 AppxBundle 包，则只会根据当前计算机上安装的 AppxBundle 包的版本号进行比对。这也是我特意留下个版本号的输入框的原因。<br>However, for AppxBundle packages, only the version number of the AppxBundle package installed on the current computer will be compared. This is why I specifically left a version number input box.</li>
</ul>

至于 Win8 应用更新到 Win8.1（AppxBundle）包，我没试过，我也不清楚。<br>As for updating Win8 applications to Win8.1 (AppxBundle) packages, I haven't tried it and I don't know.
