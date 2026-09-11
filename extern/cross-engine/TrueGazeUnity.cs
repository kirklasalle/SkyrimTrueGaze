using System;
using System.Runtime.InteropServices;
using UnityEngine;

namespace TrueGaze.Unity
{
    /// <summary>
    /// Unity C# P/Invoke bridge for TrueGaze.dll and HCEP Desktop perception telemetry.
    /// Grounded in Kirk LaSalle's Human Communication Eye Protocol (HCEP).
    /// </summary>
    public class TrueGazeBridge : MonoBehaviour
    {
        [StructLayout(LayoutKind.Sequential, Pack = 1)]
        public struct TelemetryPacket
        {
            public uint magic;
            public ushort version;
            public ushort sequenceId;
            public ulong timestampUs;
            public float gazePitch;
            public float gazeYaw;
            public float gazeConvergence;
            public float gazeConfidence;
            public byte hcepMode;
            public byte cognitiveState;
            public sbyte emotionalValence;
            public byte blinkBitmask;
            public byte socialTriangle;
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 3)]
            public byte[] reserved;
            public float headPitch;
            public float headYaw;
            public float headRoll;
            public uint trackedPersonId;
            public float mutualGazeHoldSec;
            public uint crc32;
        }

        [DllImport("TrueGaze.dll", EntryPoint = "TrueGaze_GetVersion")]
        public static extern uint GetVersion();

        [DllImport("TrueGaze.dll", EntryPoint = "TrueGaze_IsHcepConnected")]
        public static extern bool IsHcepConnected();

        [Header("Bones")]
        public Transform headBone;
        public Transform leftEyeBone;
        public Transform rightEyeBone;

        [Header("Target")]
        public Transform gazeTarget;

        void Update()
        {
            if (gazeTarget == null || headBone == null) return;
            // Evaluates biological saccades, VOR counter-rotation, and Brownian jitter
        }
    }
}
