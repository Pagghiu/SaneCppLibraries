@page library_file_system_watcher File System Watcher

@brief 🟨 Notifications {add, remove, rename, modified} for files and directories

[TOC]

SC::FileSystemWatcher allows watching directories for changes that happen to them.  

# Features
- Get notified about modified files or directories
- Get notified about added / removed / renamed files or directories

# Status
🟨 MVP  
Library does have basic capabilities and it can be used just fine.

# Description

@copydetails SC::FileSystemWatcher

# Details

The class tries to unify differences between OS specific API to deliver folder change notifications

- On Apple `FSEvents` by `CoreServices` is used.  
- On Windows `ReadDirectoryChangesW` is used.  

The behavior between these different system also depends on the file system where the watched directory resides.

# Roadmap

🟩 Usable Features:
- Implement the entire API on Linux 

🟦 Complete Features:
- Not sure what else could be useful here

💡 Unplanned Features:
- Having a thread based polling stat watcher that checks file modifications on intervals as fallback
- Allow users to provide their own thread instead of creating it behind the scenes