﻿using UnityEngine;
using UnityEditor;

namespace TestOxidize;

public class Oxidize
{
    public void BindCamera()
    {
        var c = Camera.main;
        var t = c.transform;
        var p = t.position;
        c.GetStereoViewMatrix(Camera.StereoscopicEye.Right);
        t.position = new Vector3();
    }

    public void BindSceneView()
    {
        var sv = SceneView.lastActiveSceneView;
    }
}
