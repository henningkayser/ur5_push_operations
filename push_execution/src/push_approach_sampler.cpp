#include <ur5_pusher/push_approach_sampler.h>

namespace ur5_pusher
{
	PushApproachSampler::PushApproachSampler(const float safety_range, const float emergency_range, const float min_table_distance, const float tip_length) : safety_range_(safety_range), emergency_range_(emergency_range), min_table_distance_(min_table_distance), tip_length_(tip_length) {}

	void PushApproachSampler::setMarker(const visualization_msgs::Marker& marker)
	{
		marker_ = marker;
	}

	void PushApproachSampler::setReferenceFrame(const std::string& reference_frame)
	{
		reference_frame_ = reference_frame;
	}


	bool PushApproachSampler::sampleRandomPushApproach(tams_ur5_push_execution::PushApproach& approach)
	{
		approach.frame_id = marker_.header.frame_id;
		geometry_msgs::Pose approach_pose;
		double angle = 0.0;
		if(sampleApproachPoseAndAngle(approach_pose, angle)) {
			approach.angle = angle;
			approach.point = approach_pose.position;
			approach.normal = approach_pose.orientation;
			return true;
		}
		return false;
	}

	bool PushApproachSampler::sampleApproachPoseAndAngle(geometry_msgs::Pose& pose, double& angle, int attempts) {
		geometry_msgs::PoseStamped object_pose, marker_pose;
		marker_pose.pose = marker_.pose;
		marker_pose.header.frame_id = marker_.header.frame_id;
		tf_listener_.transformPose(reference_frame_, marker_pose, object_pose);
		float distance = std::sqrt(std::pow(object_pose.pose.position.x,2) + std::pow(object_pose.pose.position.y,2));
		if(distance < safety_range_) {
			pose = sampleRandomPoseFromBox(marker_.scale.x, marker_.scale.y, marker_.scale.z);
			angle = sampleRandomPushAngle();
			return true;
		}
		if(distance < emergency_range_) {
			ROS_WARN("Object outside of SAFETY RANGE. Sampling for pushes towards table center!");
			tf::Vector3 table_vec;
			object_pose.pose.position.z = 0.0;
			tf::pointMsgToTF(object_pose.pose.position, table_vec);
			table_vec = -table_vec;

			tf::Quaternion obj_orientation;
			tf::Quaternion push_normal;
			tf::Quaternion push_direction;
			push_direction.setRPY(0.0, 0.0, angle);
			tf::quaternionMsgToTF(object_pose.pose.orientation, obj_orientation);
			tf::Vector3 push_vec(1,0,0);

			for(int i = 0; i < attempts; i++) {
				pose = sampleRandomPoseFromBox(marker_.scale.x, marker_.scale.y, marker_.scale.z);
				angle = sampleRandomPushAngle();
				tf::quaternionMsgToTF(pose.orientation, push_normal);
				tf::Vector3 push_vec(1,0,0);
				push_vec = tf::quatRotate(obj_orientation * push_normal * push_direction, push_vec);
				double angle_limit = 20.0 / 180.0 * M_PI;
				double angle_towards_table = table_vec.angle(push_vec);
				if(angle_towards_table < angle_limit || angle_towards_table > (2*M_PI - angle_limit)) {
					return true;
				}
			}
			ROS_ERROR_STREAM("Push experiment aborted! Could not sample valid push pose in " << attempts << " attempts!");
		}  else {
			ROS_ERROR_STREAM("Push experiment aborted! Object is outside of safety range.");
		}
		return false;
	}

	/**
	 * Sample random contact point from box dimensions
	 */
	geometry_msgs::Pose PushApproachSampler::sampleRandomPoseFromBox(double dim_x, double dim_y, double dim_z) {
		geometry_msgs::Pose pose;
		// Pick random value in range of perimeter
		std::uniform_real_distribution<> dis(0.0, 2 * (dim_x + dim_y));
		double p = dis(gen);
		// Match value with edge and create corresponding pose
		if(p <= dim_x) {
			pose.position.x = p;
			pose.position.y = 0;
			pose.orientation = tf::createQuaternionMsgFromYaw(0.5*M_PI);
		} else if (p <= dim_x + dim_y) {
			pose.position.x = dim_x;
			pose.position.y = p - dim_x;
			pose.orientation = tf::createQuaternionMsgFromYaw(M_PI);
		} else if (p <= 2 * dim_x + dim_y) {
			pose.position.x = 2 * dim_x + dim_y - p;
			pose.position.y = dim_y;
			pose.orientation = tf::createQuaternionMsgFromYaw(1.5*M_PI);
		} else {
			pose.position.x = 0;
			pose.position.y = 2 * (dim_x + dim_y) - p;
			pose.orientation = tf::createQuaternionMsgFromYaw(0.0);
		}
		// adjust to center
		pose.position.x -= 0.5*dim_x;
		pose.position.y -= 0.5*dim_y;

		// Pose height is related to box height and tip length
		// By default the tip aligns with the frame of the box.
		// The tip must be lifted in two mutually exclusive cases:
		// 1. The box is too small and therefore the table distance too short.
		pose.position.z = std::max(min_table_distance_ - 0.5 * dim_z, 0.0);
		// 2. The box is too high for the tip and might touch the gripper
		pose.position.z = std::max(0.5 * dim_z - tip_length_, pose.position.z);
		return pose;
	}

	float PushApproachSampler::sampleRandomPushAngle(float range) {
		std::normal_distribution<> d{0,0.5};
		return std::max(-range, std::min(range, (float)d(gen)));
	}
}
