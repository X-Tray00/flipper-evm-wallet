// SPDX-License-Identifier: MIT
pragma solidity 0.8.38;
import "../TestAssets.sol";
interface Vm {function chainId(uint256) external;function prank(address) external;function addr(uint256) external returns(address);function sign(uint256,bytes32) external returns(uint8,bytes32,bytes32);function expectRevert() external;}
contract AssetsTest {
 Vm constant vm=Vm(address(uint160(uint256(keccak256("hevm cheat code")))));
 address constant to=address(0x2222);
 function testAssetsOnAllNetworks() external {
  uint256[3] memory ids=[uint256(11155111),84532,421614];
  for(uint i;i<3;i++){
   vm.chainId(ids[i]);address owner=vm.addr(1);
   FlipperTestToken t=new FlipperTestToken();t.mint(owner,100);
   vm.prank(owner);t.transfer(to,20);require(t.balanceOf(to)==20);
   vm.prank(owner);t.approve(to,50);require(t.allowance(owner,to)==50);
   vm.prank(owner);t.approve(to,0);require(t.allowance(owner,to)==0);
   uint256 deadline=block.timestamp+1000;
   bytes32 digest=keccak256(abi.encodePacked(hex"1901",t.DOMAIN_SEPARATOR(),keccak256(abi.encode(keccak256("Permit(address owner,address spender,uint256 value,uint256 nonce,uint256 deadline)"),owner,to,12,0,deadline))));
   (uint8 v,bytes32 r,bytes32 s)=vm.sign(1,digest);t.permit(owner,to,12,deadline,v,r,s);require(t.allowance(owner,to)==12);require(t.nonces(owner)==1);
   vm.expectRevert();t.permit(owner,to,12,deadline,v,r,s);
   FlipperTestNFT nft=new FlipperTestNFT();uint256 id=nft.mint(owner);vm.prank(owner);nft.safeTransferFrom(owner,to,id);require(nft.ownerOf(id)==to);
   FlipperTestMultiToken multi=new FlipperTestMultiToken();multi.mint(owner,1,10);vm.prank(owner);multi.safeTransferFrom(owner,to,1,3,"");require(multi.balanceOf(to,1)==3);
  }
 }
 function testRejectMainnetDeployment() external {vm.chainId(1);vm.expectRevert();new FlipperTestToken();vm.expectRevert();new FlipperTestNFT();vm.expectRevert();new FlipperTestMultiToken();}
}
