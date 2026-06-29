import Foundation

// The shared store an AUv3 extension reads to learn what's unlocked. Extensions run in a
// separate sandboxed process and can't query StoreKit, so the main app writes entitlements
// into the App Group (UserDefaults suite) and each extension reads them on load
// (product-notes-followup.md §3). On a signed build this is a true cross-process container;
// the suite API works on the bare simulator too, which is what spike 5 proves.
enum AppGroup {
    static let id = "group.com.tinyjam"
    static var defaults: UserDefaults { UserDefaults(suiteName: id) ?? .standard }

    static func setUnlocked(_ ids: Set<String>) {        // main app writes
        defaults.set(Array(ids), forKey: "unlockedIDs")
    }
    static func unlocked() -> Set<String> {              // extension reads
        Set(defaults.stringArray(forKey: "unlockedIDs") ?? [])
    }

    // Diagnostic: is the real shared container present (entitlement honored)? nil on a bare
    // simulator build with no app-group entitlement — expected; the suite still works.
    static var containerAvailable: Bool {
        FileManager.default.containerURL(forSecurityApplicationGroupIdentifier: id) != nil
    }
}

// C indicator hook — how many racks the App Group reports unlocked (for the canvas dot).
@_cdecl("AppGroup_UnlockedCount")
public func AppGroup_UnlockedCount() -> Int32 { Int32(AppGroup.unlocked().count) }
