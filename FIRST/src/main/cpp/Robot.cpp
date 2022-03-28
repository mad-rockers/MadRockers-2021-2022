// Copyright (c) FIRST and other WPILib contributors.
// Open Source Software; you can modify and/or share it under the terms of
// the WPILib BSD license file in the root directory of this project.

#include "Robot.h"

#include <fmt/core.h>

#include <frc/smartdashboard/SmartDashboard.h>

void Robot::RobotInit() {
  SmartDashboard::PutNumber("Set Point", 0);
  SmartDashboard::PutNumber("Offset", 0);
  
  shooter_pid.SetP(0); //Initialize PID
  shooter_pid.SetI(0);
  shooter_pid.SetD(0);
  shooter_pid.SetFF(0.00018);
  shooter_pid.SetIZone(0);
  shooter_pid.SetOutputRange(-1.0, 1.0);
  stop_shooter();
}

/**
 * This function is called every robot packet, no matter the mode. Use
 * this for items like diagnostics that you want ran during disabled,
 * autonomous, teleoperated and test.
 *
 * <p> This runs after the mode specific periodic functions, but before
 * LiveWindow and SmartDashboard integrated updating.
 */
void Robot::RobotPeriodic() {
  SmartDashboard::PutNumber("RPM Graph", shooter_en.GetVelocity());
  SmartDashboard::PutNumber("RPM", shooter_en.GetVelocity());
}

/**
 * This autonomous (along with the chooser code above) shows how to select
 * between different autonomous modes using the dashboard. The sendable chooser
 * code works with the Java SmartDashboard. If you prefer the LabVIEW Dashboard,
 * remove all of the chooser code and uncomment the GetString line to get the
 * auto name from the text box below the Gyro.
 *
 * You can add additional auto modes by adding additional comparisons to the
 * if-else structure below with additional strings. If using the SendableChooser
 * make sure to add them to the chooser code above as well.
 */
void Robot::AutonomousInit() {
  float set_point = 0.47; //Initialize set point

  l_start = left_en.GetPosition(); //Set trackers
  r_start = right_en.GetPosition();
  driven = false;

  start_shooter(set_point); //Initialize wheels
  intake.Set(0);
  low_feed.Set(0);
  top_feed.Set(0);

  timer.Reset(); //Start timer
  timer.Start();
}

void Robot::AutonomousPeriodic() {
  float distance = 30; //Initialize auto values
  float speed = 0.2;
  float set_point = 0.47;
  
  //Start shooter PID
  if(shooter_en.GetVelocity() > 0.95 * set_point * MaxRPM) {
    start_pid();
  }

  if(!timer.HasElapsed(1.25_s) && !driven) { //Start by lowering the intake
    lift.Set(0.3);
  }
  else if(!driven) { //Start the intake wheels and reset the timer
    lift.Set(0);
    intake.Set(1);
    timer.Stop();
    timer.Reset();
    driven = true;
  }

  //Once the timer is reset, go forwards
  if(abs(left_en.GetPosition() - l_start) < distance && driven) {
    left_f.Set(speed);
    left_b.Set(speed);
  }
  else {
    left_f.Set(0);
    left_b.Set(0);
  }
  if(abs(right_en.GetPosition() - r_start) < distance && driven) {
    right_f.Set(-speed * 1.1);
    right_b.Set(-speed * 1.1);
  }
  else {
    right_f.Set(0);
    right_b.Set(0);
  }
  
  //After driving, shoot the first ball and start the timer
  if(abs(left_en.GetPosition() - l_start) > distance && 
  abs(right_en.GetPosition() - r_start) > distance && 
  timer.Get() == 0_s) {
    line_up(set_point);
    top_feed.Set(-0.8);
    timer.Start();
  }
  //Shoot the second ball
  if(timer.HasElapsed(2_s) && driven) {
    line_up(set_point);
    low_feed.Set(-0.5);
  }
}

void Robot::TeleopInit() {
  stop_shooter();
  reversed = false;
  shooting = false;
  timer.Stop();
  timer.Reset();
}

void Robot::TeleopPeriodic() {
  //Drivetrain
  float mult = 0.4; //Reduce speed
  if(reversed) {
    left_f.Set(xbox_driver.GetRightY() * mult * (1 - xbox_driver.GetRightBumper() / 2.0));
    left_b.Set(xbox_driver.GetRightY() * mult * (1 - xbox_driver.GetRightBumper() / 2.0));
    right_f.Set(-xbox_driver.GetLeftY() * mult * (1 - xbox_driver.GetLeftBumper() / 2.0));
    right_b.Set(-xbox_driver.GetLeftY() * mult * (1 - xbox_driver.GetLeftBumper() / 2.0));
  }
  else {
    left_f.Set(-xbox_driver.GetLeftY() * mult * (1 - xbox_driver.GetLeftBumper() / 2.0));
    left_b.Set(-xbox_driver.GetLeftY() * mult * (1 - xbox_driver.GetLeftBumper() / 2.0));
    right_f.Set(xbox_driver.GetRightY() * mult * (1 - xbox_driver.GetRightBumper() / 2.0));
    right_b.Set(xbox_driver.GetRightY() * mult * (1 - xbox_driver.GetRightBumper() / 2.0));
  }
  if(xbox_driver.GetXButton()) {
    while(xbox_driver.GetXButton()) {}
    reversed = !reversed;
  }

  low_feed.Set(int(xbox_operator.GetPOV() == 90) - int(xbox_operator.GetRightTriggerAxis()));// * 0.5); //Shooter system
  top_feed.Set(-int(xbox_operator.GetLeftTriggerAxis()) - int(!timer.HasElapsed(1_s) && timer.Get() != 0_s));// * 0.8);
  intake.Set(int(xbox_operator.GetPOV() == 270) - int(xbox_operator.GetPOV() == 90));
  lift.Set(int(xbox_operator.GetPOV() == 180) * 0.3 - int(xbox_operator.GetPOV() == 0) * 0.5);

  float set_point = 0.47;//SmartDashboard::GetNumber("Set Point", 0); //Shooting
  SmartDashboard::PutNumber("Expected RPM", (set_point + SmartDashboard::GetNumber("Offset", 0)) * MaxRPM);
  if(xbox_operator.GetLeftBumper()) {
    start_shooter(set_point + SmartDashboard::GetNumber("Offset", 0));
    shooting = true;
  }
  if(shooter_en.GetVelocity() > 0.95 * (set_point + SmartDashboard::GetNumber("Offset", 0)) * MaxRPM && shooting) {
    start_pid();
  }
  if(xbox_operator.GetRightBumper()) {
    stop_shooter();
    shooting = false;
  }
  if(xbox_driver.GetBButton()) {
    start_shooter(0.3);
    start_pid();
    shooting = true;
  }

  if(xbox_operator.GetBButton()) { //Shooter offset
    while(xbox_operator.GetBButton()) {}
    SmartDashboard::PutNumber("Offset", SmartDashboard::GetNumber("Offset", 0) + 0.01);
  }
  if(xbox_operator.GetXButton()) {
    while(xbox_operator.GetXButton()) {}
    SmartDashboard::PutNumber("Offset", SmartDashboard::GetNumber("Offset", 0) - 0.01);
  }

  climb1.Set(xbox_driver.GetRightTriggerAxis() - xbox_driver.GetLeftTriggerAxis());
  climb2.Set(xbox_driver.GetRightTriggerAxis() - xbox_driver.GetLeftTriggerAxis());
  SmartDashboard::PutNumber("Climb", climb_en.GetPosition());

  if(xbox_operator.GetAButton()) {
    start_shooter(set_point + SmartDashboard::GetNumber("Offset", 0));
    line_up(set_point + SmartDashboard::GetNumber("Offset", 0));
    top_feed.Set(-1);//0.8);
    timer.Stop();
    timer.Reset();
    timer.Start();
  }

  if(xbox_operator.GetStartButton()) {
    limelight_set("ledMode", 3);
  }

  if(xbox_operator.GetBackButton()) {
    limelight_set("ledMode", 1);
  }
}

void Robot::DisabledInit() {}

void Robot::DisabledPeriodic() {}

void Robot::TestInit() {}

void Robot::TestPeriodic() {}

#ifndef RUNNING_FRC_TESTS
int main() {
  return frc::StartRobot<Robot>();
}
#endif
