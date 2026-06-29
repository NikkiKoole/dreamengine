import StoreKit

// In-house StoreKit 2 manager. Apple verifies purchases on-device (JWS); no server.
// A nonisolated cache (unlockedIDs) is the bridge the C gate reads synchronously each
// frame; async StoreKit work refreshes it. The @_cdecl funcs are the C ABI surface.
final class Store {
    static let shared = Store()

    // C-readable snapshot of owned product IDs (gate reads this; StoreKit refreshes it).
    // Plain static (Swift 5.9 has no nonisolated(unsafe)); a gate read tolerates the race.
    static private(set) var unlockedIDs: Set<String> = []

    let ids = ["com.tinyjam.rebirth", "com.tinyjam.funk", "com.tinyjam.masterpass"]
    private var products: [String: Product] = [:]
    private var updatesTask: Task<Void, Never>?

    func start() async {
        if updatesTask == nil { updatesTask = listen() }
        await load()
        await refresh()
    }

    func load() async {
        do { for p in try await Product.products(for: ids) { products[p.id] = p } }
        catch { NSLog("[store] load failed: %@", "\(error)") }
    }

    func purchase(_ id: String) async {
        guard let p = products[id] else { NSLog("[store] no such product %@", id); return }
        do {
            if case .success(let v) = try await p.purchase(), case .verified(let t) = v {
                await t.finish()
                await refresh()
            }
        } catch { NSLog("[store] purchase failed: %@", "\(error)") }
    }

    func refresh() async {
        var owned = Set<String>()
        for await r in Transaction.currentEntitlements {
            if case .verified(let t) = r { owned.insert(t.productID) }
        }
        Store.unlockedIDs = owned
    }

    static func isUnlocked(_ id: String) -> Bool {
        unlockedIDs.contains(id) || unlockedIDs.contains("com.tinyjam.masterpass")
    }

    private func listen() -> Task<Void, Never> {
        Task.detached {
            for await r in Transaction.updates {
                if case .verified(let t) = r { await t.finish(); await Store.shared.refresh() }
            }
        }
    }
}

// ── C bridge (tinyjam_store.h) ───────────────────────────────────────────────
@_cdecl("Store_Init")
public func Store_Init() { Task { await Store.shared.start() } }

@_cdecl("Store_IsModuleUnlocked")
public func Store_IsModuleUnlocked(_ moduleId: UnsafePointer<CChar>) -> Bool {
    Store.isUnlocked(String(cString: moduleId))
}

@_cdecl("Store_Purchase")
public func Store_Purchase(_ moduleId: UnsafePointer<CChar>) {
    let id = String(cString: moduleId)
    Task { await Store.shared.purchase(id) }
}
