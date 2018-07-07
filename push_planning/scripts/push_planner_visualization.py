#!/usr/bin/env python

import rospy
from tams_ur5_push_execution.msg import PushTrajectory 
from visualization_msgs.msg import Marker, MarkerArray
from std_msgs.msg import ColorRGBA
from geometry_msgs.msg import Pose, Vector3
from graph_msgs.msg import GeometryGraph, Edges

from lib.marker_helper import init_marker, init_graph_markers

import actionlib
from push_planning.msg import PushPlanAction, PushPlanGoal

dim_X = 0.162
dim_Y = 0.23
dim_Z = 0.112

def visualize_object_trajectory(traj_msg):
    global marker_pub, data_pub
    markers = MarkerArray()

    # visualize as graph
    nodes = [p.position for p in traj_msg.poses]
    edges = [Edges([i, i+1], [1.0, 1.0]) for i in range(len(nodes)-1)]
    edges.append(Edges())
    color = ColorRGBA(0.0,1.0,0.0,1.0)
    #visualize_graph(nodes, edges, nodes_id=2, edges_id=3, nodes_color=color, edges_color=color, linewidth=0.003, is_path=True)
    pose = Pose()
    pose.position.z = 0.5*dim_Z
    pose.orientation.w = 1.0
    points, lines = init_graph_markers(nodes, edges, nodes_id=2, edges_id=3, pose=pose, nodes_color=color, edges_color=color, linewidth=0.003, is_path=True)
    data_pub.publish(points)
    data_pub.publish(lines)

    # visualize object path with markers
    # TODO: we might use the existing marker to copy the actual shape instead of using hard coded values
    for i,pose in enumerate(traj_msg.poses):
        progress = float(i) / len(traj_msg.poses)
        color = ColorRGBA(progress,0.0,1-progress,0.1)
        scale = Vector3(dim_X, dim_Y, dim_Z)
        pose.position.z = 0.5*dim_Z
        markers.markers.append(init_marker(i, Marker.CUBE, scale=scale, pose=pose, color=color))

    marker_pub.publish(markers)

def visualize_planner_data(graph_msg):
    global data_pub
    #visualize_graph(graph_msg.nodes, graph_msg.edges)
    pose = Pose()
    pose.position.z = 0.5*dim_Z
    pose.orientation.w = 1.0
    points, lines = init_graph_markers(graph_msg.nodes, graph_msg.edges, pose=pose)
    data_pub.publish(points)
    data_pub.publish(lines)


def visualization_node():
    global marker_pub, data_pub, planner_client
    rospy.init_node('push_trajectory_visualization_node');

    traj_sub = rospy.Subscriber('/push_trajectory', PushTrajectory, visualize_object_trajectory, queue_size=1)
    data_sub = rospy.Subscriber('/push_planner_graph', GeometryGraph, visualize_planner_data, queue_size=1)

    marker_pub = rospy.Publisher("/push_trajectory_markers", MarkerArray, queue_size=1)
    data_pub = rospy.Publisher("/push_planner_graph_markers", Marker, queue_size=1)
    graph_pub = rospy.Publisher("/push_planner_graph_markers", Marker, queue_size=10)

    planner_client = actionlib.SimpleActionClient('/push_plan_action', PushPlanAction)

    print "Ready to visualize push trajectories"
    rospy.spin()

def call_push_plan_action(object_id, start_pose, goal_pose):
    global planner_client

    #create goal
    goal = PushPlanGoal()
    goal.object_id = object_id
    goal.start_pose = start_pose
    goal.goal_pose = goal_pose

    # call client
    planner_client.wait_for_server()
    planner_client.send_goal(goal)
    planner_client.wait_for_result()

    # receive result
    result = planner_client.get_result()



if __name__=="__main__":
    print "init push planner visualization node"
    visualization_node()
