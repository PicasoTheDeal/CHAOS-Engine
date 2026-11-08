import QtQuick
import org.kde.taskmanager as TaskManager

// what is over the desktop on this screen and desktop: 2 a fullscreen window, 1 a maximised one, 0 nothing.
// lives in its own file behind a Loader so a plasma without the taskmanager module just leaves coverage at 0
Item {
    id: root
    property int coverage: 0
    property var screenGeometry
    TaskManager.VirtualDesktopInfo { id: vdi }
    TaskManager.ActivityInfo { id: ai }
    TaskManager.TasksModel {
        id: tasks
        filterByScreen: true; screenGeometry: root.screenGeometry
        filterByVirtualDesktop: true; virtualDesktop: vdi.currentDesktop
        filterByActivity: true; activity: ai.currentActivity
        filterMinimized: true
        onDataChanged: timer.restart()
        onRowsInserted: timer.restart()
        onRowsRemoved: timer.restart()
    }
    Timer { id: timer; interval: 400; onTriggered: root.recount() }
    Timer { interval: 2000; running: true; repeat: true; onTriggered: root.recount() }
    function recount() {
        let c = 0
        for (let i = 0; i < tasks.count; i++) {
            const idx = tasks.index(i, 0)
            if (tasks.data(idx, TaskManager.AbstractTasksModel.IsMinimized) === true) continue
            if (tasks.data(idx, TaskManager.AbstractTasksModel.IsFullScreen) === true) { c = 2; break }
            if (tasks.data(idx, TaskManager.AbstractTasksModel.IsMaximized) === true) c = 1
        }
        if (c !== root.coverage) root.coverage = c
    }
}
