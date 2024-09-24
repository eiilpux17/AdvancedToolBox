# AdvancedToolBox

由于QToolBox不支持同时展开和折叠tab，功能比较弱。所以用Qt重新实现了一个更好的tool box，仅支持垂直布局。

![toolbox_preview](https://img.picgo.net/2024/09/24/advance_tool_box18b7a681dcc1cb84.gif#=350x)

### 支持特性：

* 每个tab页支持展开和折叠

* 可鼠标移动handle调整tab大小(类似QSplitter)

* 可以拖拽tab标题重排tab

* 可以通过style sheet设置tab标题、separator handle、expanding icon等样式

### 布局实现

AdvancedToolBox内部使用手动布局，每个标签页区域有三个元素：separator、title、container。

* separator，可以通过style sheet设置颜色等，可以通过鼠标拖拽调整相关tab的尺寸

* title，主要绘制展开或折叠状态、图标、标题文字，点击可以折叠和展开，展开和折叠设置了动画

* container，用户设置的Widget的容器，使用这层容器的目的是为了在展开或折叠时，避免过多的resize event。

考虑到需要拖拽排序，每个标签页区域没有使用独立布局，AdvancedToolBox窗口触发布局时，对每个标签页的三个元素按顺序计算高度并布局。


### 待支持功能

- [ ] 增加展开和折叠时信号  
- [ ] 标签页标题右侧支持自定义QAction
- [ ] 展开和折叠时，应该触发widget的show和hide事件
