require "test_helper"

class DeviceMembershipTest < ActiveSupport::TestCase
  setup do
    @owner = User.create!(email: "role-owner@example.com", password: "password1")
    @device = Device.create!(id: "role-device", user: @owner, password: "device-pass-1")
  end

  test "device registration creates a resident membership" do
    assert_equal "resident", @device.device_memberships.find_by!(user: @owner).role
    assert_equal "resident", @device.role_for(@owner)
    assert @device.resident?(@owner)
    assert_not @device.messenger?(@owner)
  end

  test "joining again does not replace an existing role" do
    member = User.create!(email: "role-member@example.com", password: "password1")
    @device.grant_access_to!(member, role: "messenger")

    @device.grant_access_to!(member)

    assert_equal "messenger", @device.device_memberships.find_by!(user: member).role
    assert_equal "messenger", @device.role_for(member)
    assert @device.messenger?(member)
    assert_not @device.resident?(member)
  end

  test "rejects an unknown role" do
    membership = @device.device_memberships.find_by!(user: @owner)
    membership.role = "guest"

    assert_not membership.valid?
  end
end
