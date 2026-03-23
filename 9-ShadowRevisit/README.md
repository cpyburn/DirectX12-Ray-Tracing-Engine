<img width="1282" height="752" alt="image" src="https://github.com/user-attachments/assets/9d7ed25a-f87a-4a1b-99cc-4b5697ccadf6" />


Up until now we have been doing a little bit of wonky stuff, like adding a triangle geom in with the plane geom, in real practice, you may do something like this to make 1 static model, but usually each model would have its on BLAS. We are going to head that direction with this tutorial.

We will also add a shadow ray to the models and update the HLSL to use normals.

We will also add a future buffer that is the frame index + 1 and launch that on a async thread. It should always be ready for rendering.
