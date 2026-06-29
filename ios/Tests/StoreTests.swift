import XCTest
import StoreKitTest
@testable import TinyjamHello

// Headless proof of the IAP model — no Apple account, no network. SKTestSession loads
// the local Tinyjam.storekit config; we buy a product and assert the entitlement flips.
final class StoreTests: XCTestCase {
    var session: SKTestSession!

    override func setUpWithError() throws {
        session = try SKTestSession(configurationFileNamed: "Tinyjam")
        session.disableDialogs = true     // auto-approve the purchase sheet
        session.clearTransactions()
    }

    func testPurchaseUnlocksModule() async throws {
        await Store.shared.start()
        XCTAssertFalse(Store.isUnlocked("com.tinyjam.rebirth"), "should start locked")

        await Store.shared.purchase("com.tinyjam.rebirth")
        XCTAssertTrue(Store.isUnlocked("com.tinyjam.rebirth"), "should unlock after purchase")
        XCTAssertFalse(Store.isUnlocked("com.tinyjam.funk"), "other racks stay locked")
    }

    func testMasterPassUnlocksEverything() async throws {
        await Store.shared.start()
        await Store.shared.purchase("com.tinyjam.masterpass")
        XCTAssertTrue(Store.isUnlocked("com.tinyjam.funk"), "master pass unlocks all")
        XCTAssertTrue(Store.isUnlocked("com.tinyjam.rebirth"), "master pass unlocks all")
    }
}
