import SwiftUI

@main
struct PendantApp: App {
    @StateObject private var model = AppModel()
    @Environment(\.scenePhase) private var phase

    var body: some Scene {
        WindowGroup {
            ContentView().environmentObject(model)
        }
        .onChange(of: phase) { _, p in model.controller.setForeground(fg: p == .active) }
    }
}
