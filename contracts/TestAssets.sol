// SPDX-License-Identifier: MIT
pragma solidity 0.8.38;
import "@openzeppelin/contracts/token/ERC20/extensions/ERC20Permit.sol";
import "@openzeppelin/contracts/token/ERC721/ERC721.sol";
import "@openzeppelin/contracts/token/ERC1155/ERC1155.sol";
abstract contract TestnetOnly {
    constructor() {
        require(block.chainid == 11155111 || block.chainid == 84532 || block.chainid == 421614, "testnet only");
    }
}
/// @notice Valueless test token. Anyone can mint. Never represent as a production asset.
contract FlipperTestToken is ERC20Permit, TestnetOnly {
    constructor() ERC20("Flipper Test Token", "FEWT") ERC20Permit("Flipper Test Token") {}
    function mint(address to, uint256 value) external { _mint(to, value); }
}
/// @notice Public test NFT faucet; no external metadata URLs.
contract FlipperTestNFT is ERC721, TestnetOnly {
    uint256 public nextId;
    constructor() ERC721("Flipper Test NFT", "FEWN") {}
    function mint(address to) external returns (uint256 id) { id = nextId++; _safeMint(to, id); }
}
/// @notice Public test multi-token faucet; no external metadata URLs.
contract FlipperTestMultiToken is ERC1155, TestnetOnly {
    constructor() ERC1155("") {}
    function mint(address to, uint256 id, uint256 value) external { _mint(to, id, value, ""); }
}
