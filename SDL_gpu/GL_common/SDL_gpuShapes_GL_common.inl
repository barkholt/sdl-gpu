/* This is an implementation file to be included after certain #defines have been set.
See a particular renderer's *.c file for specifics. */

#ifndef DEGPERRAD
#define DEGPERRAD 57.2957795f
#endif

#ifndef RADPERDEG
#define RADPERDEG 0.0174532925f
#endif







// All shapes start this way for setup and so they can access the blit buffer properly
#define BEGIN_UNTEXTURED(function_name, shape, num_additional_vertices, num_additional_indices) \
    if(target == NULL) \
    { \
        GPU_PushErrorCode(function_name, GPU_ERROR_NULL_ARGUMENT, "target"); \
        return; \
    } \
    if(renderer != target->renderer) \
    { \
        GPU_PushErrorCode(function_name, GPU_ERROR_USER_ERROR, "Mismatched renderer"); \
        return; \
    } \
     \
    makeContextCurrent(renderer, target); \
    if(renderer->current_context_target == NULL) \
    { \
        GPU_PushErrorCode(function_name, GPU_ERROR_USER_ERROR, "NULL context"); \
        return; \
    } \
     \
    if(!bindFramebuffer(renderer, target)) \
    { \
        GPU_PushErrorCode(function_name, GPU_ERROR_BACKEND_ERROR, "Failed to bind framebuffer."); \
        return; \
    } \
     \
    prepareToRenderToTarget(renderer, target); \
    prepareToRenderShapes(renderer, shape); \
     \
    GPU_CONTEXT_DATA* cdata = (GPU_CONTEXT_DATA*)renderer->current_context_target->context->data; \
     \
    if(cdata->blit_buffer_num_vertices + (num_additional_vertices) >= cdata->blit_buffer_max_num_vertices) \
    { \
        if(!growBlitBuffer(cdata, cdata->blit_buffer_num_vertices + (num_additional_vertices))) \
            renderer->FlushBlitBuffer(renderer); \
    } \
    if(cdata->index_buffer_num_vertices + (num_additional_indices) >= cdata->index_buffer_max_num_vertices) \
    { \
        if(!growIndexBuffer(cdata, cdata->index_buffer_num_vertices + (num_additional_indices))) \
            renderer->FlushBlitBuffer(renderer); \
    } \
     \
    float* blit_buffer = cdata->blit_buffer; \
    unsigned short* index_buffer = cdata->index_buffer; \
     \
    int vert_index = GPU_BLIT_BUFFER_VERTEX_OFFSET + cdata->blit_buffer_num_vertices*GPU_BLIT_BUFFER_FLOATS_PER_VERTEX; \
    int color_index = GPU_BLIT_BUFFER_COLOR_OFFSET + cdata->blit_buffer_num_vertices*GPU_BLIT_BUFFER_FLOATS_PER_VERTEX; \
     \
    float r, g, b, a; \
    if(target->use_color) \
    { \
        r = MIX_COLOR_COMPONENT_NORMALIZED_RESULT(target->color.r, color.r); \
        g = MIX_COLOR_COMPONENT_NORMALIZED_RESULT(target->color.g, color.g); \
        b = MIX_COLOR_COMPONENT_NORMALIZED_RESULT(target->color.b, color.b); \
        a = MIX_COLOR_COMPONENT_NORMALIZED_RESULT(GET_ALPHA(target->color), GET_ALPHA(color)); \
    } \
    else \
    { \
        r = color.r/255.0f; \
        g = color.g/255.0f; \
        b = color.b/255.0f; \
        a = GET_ALPHA(color)/255.0f; \
    } \
    unsigned short blit_buffer_starting_index = cdata->blit_buffer_num_vertices; \
    (void)blit_buffer_starting_index;





static float SetLineThickness(GPU_Renderer* renderer, float thickness)
{
    if(renderer->current_context_target == NULL)
        return 1.0f;
    
	float old = renderer->current_context_target->context->line_thickness;
	if(old != thickness)
        renderer->FlushBlitBuffer(renderer);
    
	renderer->current_context_target->context->line_thickness = thickness;
	glLineWidth(thickness);
	return old;
}

static float GetLineThickness(GPU_Renderer* renderer)
{
    return renderer->current_context_target->context->line_thickness;
}

static void Pixel(GPU_Renderer* renderer, GPU_Target* target, float x, float y, SDL_Color color)
{
    BEGIN_UNTEXTURED("GPU_Pixel", GL_POINTS, 1, 1);
    
    SET_UNTEXTURED_VERTEX(x, y, r, g, b, a);
}

static void Line(GPU_Renderer* renderer, GPU_Target* target, float x1, float y1, float x2, float y2, SDL_Color color)
{
    BEGIN_UNTEXTURED("GPU_Line", GL_LINES, 2, 2);
    
    SET_UNTEXTURED_VERTEX(x1, y1, r, g, b, a);
    SET_UNTEXTURED_VERTEX(x2, y2, r, g, b, a);
}

// Arc() might call Circle()
static void Circle(GPU_Renderer* renderer, GPU_Target* target, float x, float y, float radius, SDL_Color color);

static void Arc(GPU_Renderer* renderer, GPU_Target* target, float x, float y, float radius, float startAngle, float endAngle, SDL_Color color)
{
    float originalSA = startAngle;

    if(startAngle > endAngle)
    {
        float swapa = endAngle;
        endAngle = startAngle;
        startAngle = swapa;
    }
    if(startAngle == endAngle)
        return;

    // Big angle
    if(endAngle - startAngle >= 360)
    {
        Circle(renderer, target, x, y, radius, color);
        return;
    }

    // Shift together
    while(startAngle < 0 && endAngle < 0)
    {
        startAngle += 360;
        endAngle += 360;
    }
    while(startAngle > 360 && endAngle > 360)
    {
        startAngle -= 360;
        endAngle -= 360;
    }

    // Check if the angle to be drawn crosses 0
    Uint8 crossesZero = (startAngle < 0 && endAngle > 0) || (startAngle < 360 && endAngle > 360);

    if(endAngle == 0)
        endAngle = 360;
    else if(crossesZero)
    {
        float sa = originalSA;
        // Render the left part
        while(sa < 0.0f)
            sa += 360;
        Arc(renderer, target, x, y, radius, sa, 359.9f, color);

        // Continue to render the right part
        startAngle = 0;
        while(endAngle >= 360)
            endAngle -= 360;
    }


    float t = startAngle;
    float dt = ((endAngle - startAngle)/360)*(1.25f/sqrtf(radius)) * DEGPERRAD;  // s = rA, so dA = ds/r.  ds of 1.25*sqrt(radius) is good, use A in degrees.
    float dx, dy;

    int numSegments = fabs(endAngle - startAngle)/dt;
    if(numSegments == 0)
        return;
    
    BEGIN_UNTEXTURED("GPU_Arc", GL_LINES, 1 + (numSegments-1) + 1, 1 + (numSegments-1)*2 + 1);
    
    dx = radius*cos(t*RADPERDEG);
    dy = radius*sin(t*RADPERDEG);
    SET_UNTEXTURED_VERTEX(x+dx, y+dy, r, g, b, a); // first point
    t += dt;

    int i;
    for(i = 1; i < numSegments; i++)
    {
        dx = radius*cos(t*RADPERDEG);
        dy = radius*sin(t*RADPERDEG);
        SET_UNTEXTURED_VERTEX(x+dx, y+dy, r, g, b, a);
        SET_INDEXED_VERTEX(i);  // Double that vertex
        t += dt;
    }
    
    // Last point
    dx = radius*cos(endAngle*RADPERDEG);
    dy = radius*sin(endAngle*RADPERDEG);
    SET_UNTEXTURED_VERTEX(x+dx, y+dy, r, g, b, a);
}

// ArcFilled() might call CircleFilled()
static void CircleFilled(GPU_Renderer* renderer, GPU_Target* target, float x, float y, float radius, SDL_Color color);

static void ArcFilled(GPU_Renderer* renderer, GPU_Target* target, float x, float y, float radius, float startAngle, float endAngle, SDL_Color color)
{
    float originalSA = startAngle;

    if(startAngle > endAngle)
    {
        float swapa = endAngle;
        endAngle = startAngle;
        startAngle = swapa;
    }
    if(startAngle == endAngle)
        return;

    // Big angle
    if(endAngle - startAngle >= 360)
    {
        CircleFilled(renderer, target, x, y, radius, color);
        return;
    }

    // Shift together
    while(startAngle < 0 && endAngle < 0)
    {
        startAngle += 360;
        endAngle += 360;
    }
    while(startAngle > 360 && endAngle > 360)
    {
        startAngle -= 360;
        endAngle -= 360;
    }

    // Check if the angle to be drawn crosses 0
    Uint8 crossesZero = (startAngle < 0 && endAngle > 0) || (startAngle < 360 && endAngle > 360);

    if(endAngle == 0)
        endAngle = 360;
    else if(crossesZero)
    {
        float sa = originalSA;

        // Render the left part
        while(sa < 0.0f)
            sa += 360;
        ArcFilled(renderer, target, x, y, radius, sa, 359.9f, color);

        // Continue to render the right part
        startAngle = 0;
        while(endAngle >= 360)
            endAngle -= 360;
    }
    
    float t = startAngle;
    float dt = ((endAngle - startAngle)/360)*(1.25f/sqrtf(radius)) * DEGPERRAD;  // s = rA, so dA = ds/r.  ds of 1.25*sqrt(radius) is good, use A in degrees.
    float dx, dy;

    int numSegments = fabs(endAngle - startAngle)/dt;
    if(numSegments == 0)
        return;


    BEGIN_UNTEXTURED("GPU_ArcFilled", GL_TRIANGLES, 3 + (numSegments-1) + 1, 3 + (numSegments-1)*3 + 3);

    // First triangle
    SET_UNTEXTURED_VERTEX(x, y, r, g, b, a);
    dx = radius*cos(t*RADPERDEG);
    dy = radius*sin(t*RADPERDEG);
    SET_UNTEXTURED_VERTEX(x+dx, y+dy, r, g, b, a); // first point
    t += dt;
    dx = radius*cos(t*RADPERDEG);
    dy = radius*sin(t*RADPERDEG);
    SET_UNTEXTURED_VERTEX(x+dx, y+dy, r, g, b, a); // new point
    t += dt;
    
    int i;
    for(i = 2; i < numSegments+1; i++)
    {
        dx = radius*cos(t*RADPERDEG);
        dy = radius*sin(t*RADPERDEG);
        SET_INDEXED_VERTEX(0);  // center
        SET_INDEXED_VERTEX(i);  // last point
        SET_UNTEXTURED_VERTEX(x+dx, y+dy, r, g, b, a); // new point
        t += dt;
    }
    
    // Last triangle
    dx = radius*cos(endAngle*RADPERDEG);
    dy = radius*sin(endAngle*RADPERDEG);
    SET_INDEXED_VERTEX(0);  // center
    SET_INDEXED_VERTEX(i);  // last point
    SET_UNTEXTURED_VERTEX(x+dx, y+dy, r, g, b, a); // new point
}

static void Circle(GPU_Renderer* renderer, GPU_Target* target, float x, float y, float radius, SDL_Color color)
{
    float t = 0;
    float dt = (1.25f/sqrtf(radius)) * DEGPERRAD;  // s = rA, so dA = ds/r.  ds of 1.25*sqrt(radius) is good, use A in degrees.
    float dx, dy;
    int numSegments = 360/dt+1;
    
    BEGIN_UNTEXTURED("GPU_Circle", GL_LINES, 1 + (numSegments-1), 1 + (numSegments-1)*2 + 1);

    dx = radius;
    dy = 0.0f;
    SET_UNTEXTURED_VERTEX(x+dx, y+dy, r, g, b, a); // first point
    
    int i;
    for(i = 1; i < numSegments; i++)
    {
        dx = radius*cos(t*RADPERDEG);
        dy = radius*sin(t*RADPERDEG);
        SET_UNTEXTURED_VERTEX(x+dx, y+dy, r, g, b, a);
        SET_INDEXED_VERTEX(i);  // Double that vertex
        t += dt;
    }
    
    SET_INDEXED_VERTEX(0);  // back to the beginning
}

static void CircleFilled(GPU_Renderer* renderer, GPU_Target* target, float x, float y, float radius, SDL_Color color)
{
    float t = 0;
    float dt = (1.25f/sqrtf(radius)) * DEGPERRAD;  // s = rA, so dA = ds/r.  ds of 1.25*sqrt(radius) is good, use A in degrees.
    float dx, dy;

    int numSegments = 360/dt+1;
    
    BEGIN_UNTEXTURED("GPU_CircleFilled", GL_TRIANGLES, 3 + (numSegments-2), 3 + (numSegments-2)*3 + 3);

    // First triangle
    SET_UNTEXTURED_VERTEX(x, y, r, g, b, a);
    dx = radius;
    dy = 0.0f;
    SET_UNTEXTURED_VERTEX(x+dx, y+dy, r, g, b, a); // first point
    t += dt;
    dx = radius*cos(t*RADPERDEG);
    dy = radius*sin(t*RADPERDEG);
    SET_UNTEXTURED_VERTEX(x+dx, y+dy, r, g, b, a); // new point
    t += dt;
    
    int i;
    for(i = 2; i < numSegments; i++)
    {
        dx = radius*cos(t*RADPERDEG);
        dy = radius*sin(t*RADPERDEG);
        SET_INDEXED_VERTEX(0);  // center
        SET_INDEXED_VERTEX(i);  // last point
        SET_UNTEXTURED_VERTEX(x+dx, y+dy, r, g, b, a); // new point
        t += dt;
    }
    
    SET_INDEXED_VERTEX(0);  // center
    SET_INDEXED_VERTEX(i);  // last point
    SET_INDEXED_VERTEX(1);  // first point
}

static void Tri(GPU_Renderer* renderer, GPU_Target* target, float x1, float y1, float x2, float y2, float x3, float y3, SDL_Color color)
{
    BEGIN_UNTEXTURED("GPU_Tri", GL_LINES, 3, 6);
    
    SET_UNTEXTURED_VERTEX(x1, y1, r, g, b, a);
    SET_UNTEXTURED_VERTEX(x2, y2, r, g, b, a);
    
    SET_INDEXED_VERTEX(1);
    SET_UNTEXTURED_VERTEX(x3, y3, r, g, b, a);
    
    SET_INDEXED_VERTEX(2);
    SET_INDEXED_VERTEX(0);
}

static void TriFilled(GPU_Renderer* renderer, GPU_Target* target, float x1, float y1, float x2, float y2, float x3, float y3, SDL_Color color)
{
    BEGIN_UNTEXTURED("GPU_TriFilled", GL_TRIANGLES, 3, 3);
    
    SET_UNTEXTURED_VERTEX(x1, y1, r, g, b, a);
    SET_UNTEXTURED_VERTEX(x2, y2, r, g, b, a);
    SET_UNTEXTURED_VERTEX(x3, y3, r, g, b, a);
}

static void Rectangle(GPU_Renderer* renderer, GPU_Target* target, float x1, float y1, float x2, float y2, SDL_Color color)
{
    if(y2 < y1)
    {
        float y = y1;
        y1 = y2;
        y2 = y;
    }
    if(x2 < x1)
    {
        float x = x1;
        x1 = x2;
        x2 = x;
    }
    
	float thickness = renderer->GetLineThickness(renderer);

    float t = thickness/2;
    
    // Thick lines via filled triangles
    
    BEGIN_UNTEXTURED("GPU_Rectangle", GL_TRIANGLES, 10, 24);
    
    // First triangle
    SET_UNTEXTURED_VERTEX(x1 - t, y1 - t, r, g, b, a);
    SET_UNTEXTURED_VERTEX(x1 + t, y1 + t, r, g, b, a);
    SET_UNTEXTURED_VERTEX(x2 + t, y1 - t, r, g, b, a);
    
    SET_INDEXED_VERTEX(1);
    SET_INDEXED_VERTEX(2);
    SET_UNTEXTURED_VERTEX(x2 - t, y1 + t, r, g, b, a);
    
    SET_INDEXED_VERTEX(2);
    SET_INDEXED_VERTEX(3);
    SET_UNTEXTURED_VERTEX(x2 + t, y2 + t, r, g, b, a);
    
    SET_INDEXED_VERTEX(3);
    SET_INDEXED_VERTEX(4);
    SET_UNTEXTURED_VERTEX(x2 - t, y2 - t, r, g, b, a);
    
    SET_INDEXED_VERTEX(4);
    SET_INDEXED_VERTEX(5);
    SET_UNTEXTURED_VERTEX(x1 - t, y2 + t, r, g, b, a);
    
    SET_INDEXED_VERTEX(5);
    SET_INDEXED_VERTEX(6);
    SET_UNTEXTURED_VERTEX(x1 + t, y2 - t, r, g, b, a);
    
    SET_INDEXED_VERTEX(6);
    SET_INDEXED_VERTEX(7);
    SET_UNTEXTURED_VERTEX(x1 - t, y1 - t, r, g, b, a);
    
    SET_INDEXED_VERTEX(7);
    SET_INDEXED_VERTEX(8);
    SET_UNTEXTURED_VERTEX(x1 + t, y1 + t, r, g, b, a);
}

static void RectangleFilled(GPU_Renderer* renderer, GPU_Target* target, float x1, float y1, float x2, float y2, SDL_Color color)
{
    BEGIN_UNTEXTURED("GPU_RectangleFilled", GL_TRIANGLES, 4, 6);

    SET_UNTEXTURED_VERTEX(x1, y1, r, g, b, a);
    SET_UNTEXTURED_VERTEX(x1, y2, r, g, b, a);
    SET_UNTEXTURED_VERTEX(x2, y1, r, g, b, a);
    
    SET_INDEXED_VERTEX(1);
    SET_INDEXED_VERTEX(2);
    SET_UNTEXTURED_VERTEX(x2, y2, r, g, b, a);
}

static void RectangleRound(GPU_Renderer* renderer, GPU_Target* target, float x1, float y1, float x2, float y2, float radius, SDL_Color color)
{
    if(y2 < y1)
    {
        float temp = y2;
        y2 = y1;
        y1 = temp;
    }
    if(x2 < x1)
    {
        float temp = x2;
        x2 = x1;
        x1 = temp;
    }
    
    if(radius > (x2-x1)/2)
        radius = (x2-x1)/2;
    if(radius > (y2-y1)/2)
        radius = (y2-y1)/2;
    
    float tau = 2*M_PI;
    
    int verts_per_corner = 7;
    float corner_angle_increment = (tau/4)/(verts_per_corner-1);  // 0, 15, 30, 45, 60, 75, 90
    
    BEGIN_UNTEXTURED("GPU_RectangleRound", GL_LINES, 4 + 4*(verts_per_corner-1), 8 + 4*(verts_per_corner-1)*2);
    
    // Starting angle
    float angle = tau*0.75f;
    
    // First point
    SET_UNTEXTURED_VERTEX(x2-radius+cos(angle)*radius, y1+radius+sin(angle)*radius, r, g, b, a);
    
    int last_index = 1;
    int i;
    for(i = 1; i < verts_per_corner; i++)
    {
        SET_UNTEXTURED_VERTEX(x2-radius+cos(angle)*radius, y1+radius+sin(angle)*radius, r, g, b, a);
        SET_INDEXED_VERTEX(last_index++);
        angle += corner_angle_increment;
    }
    
    SET_UNTEXTURED_VERTEX(x2-radius+cos(angle)*radius, y2-radius+sin(angle)*radius, r, g, b, a);
    SET_INDEXED_VERTEX(last_index++);
    for(i = 1; i < verts_per_corner; i++)
    {
        SET_UNTEXTURED_VERTEX(x2-radius+cos(angle)*radius, y2-radius+sin(angle)*radius, r, g, b, a);
        SET_INDEXED_VERTEX(last_index++);
        angle += corner_angle_increment;
    }
    
    SET_UNTEXTURED_VERTEX(x1+radius+cos(angle)*radius, y2-radius+sin(angle)*radius, r, g, b, a);
    SET_INDEXED_VERTEX(last_index++);
    for(i = 1; i < verts_per_corner; i++)
    {
        SET_UNTEXTURED_VERTEX(x1+radius+cos(angle)*radius, y2-radius+sin(angle)*radius, r, g, b, a);
        SET_INDEXED_VERTEX(last_index++);
        angle += corner_angle_increment;
    }
    
    SET_UNTEXTURED_VERTEX(x1+radius+cos(angle)*radius, y1+radius+sin(angle)*radius, r, g, b, a);
    SET_INDEXED_VERTEX(last_index++);
    for(i = 1; i < verts_per_corner; i++)
    {
        SET_UNTEXTURED_VERTEX(x1+radius+cos(angle)*radius, y1+radius+sin(angle)*radius, r, g, b, a);
        SET_INDEXED_VERTEX(last_index++);
        angle += corner_angle_increment;
    }
    
    // Last point
    SET_INDEXED_VERTEX(0);
}

static void RectangleRoundFilled(GPU_Renderer* renderer, GPU_Target* target, float x1, float y1, float x2, float y2, float radius, SDL_Color color)
{
    if(y2 < y1)
    {
        float temp = y2;
        y2 = y1;
        y1 = temp;
    }
    if(x2 < x1)
    {
        float temp = x2;
        x2 = x1;
        x1 = temp;
    }
    
    if(radius > (x2-x1)/2)
        radius = (x2-x1)/2;
    if(radius > (y2-y1)/2)
        radius = (y2-y1)/2;
    
    float tau = 2*M_PI;
    
    int verts_per_corner = 7;
    float corner_angle_increment = (tau/4)/(verts_per_corner-1);  // 0, 15, 30, 45, 60, 75, 90
    
    BEGIN_UNTEXTURED("GPU_RectangleRoundFilled", GL_TRIANGLES, 6 + 4*(verts_per_corner-1) - 1, 15 + 4*(verts_per_corner-1)*3 - 3);
    
    // Starting angle
    float angle = tau*0.75f;
    
    // First triangle
    SET_UNTEXTURED_VERTEX((x2+x1)/2, (y2+y1)/2, r, g, b, a);  // Center
    SET_UNTEXTURED_VERTEX(x2-radius+cos(angle)*radius, y1+radius+sin(angle)*radius, r, g, b, a);
    angle += corner_angle_increment;
    SET_UNTEXTURED_VERTEX(x2-radius+cos(angle)*radius, y1+radius+sin(angle)*radius, r, g, b, a);
    angle += corner_angle_increment;
    
    int last_index = 2;
    int i;
    for(i = 2; i < verts_per_corner; i++)
    {
        SET_INDEXED_VERTEX(0);
        SET_INDEXED_VERTEX(last_index++);
        SET_UNTEXTURED_VERTEX(x2-radius+cos(angle)*radius, y1+radius+sin(angle)*radius, r, g, b, a);
        angle += corner_angle_increment;
    }
    
    SET_INDEXED_VERTEX(0);
    SET_INDEXED_VERTEX(last_index++);
    SET_UNTEXTURED_VERTEX(x2-radius+cos(angle)*radius, y2-radius+sin(angle)*radius, r, g, b, a);
    for(i = 1; i < verts_per_corner; i++)
    {
        SET_INDEXED_VERTEX(0);
        SET_INDEXED_VERTEX(last_index++);
        SET_UNTEXTURED_VERTEX(x2-radius+cos(angle)*radius, y2-radius+sin(angle)*radius, r, g, b, a);
        angle += corner_angle_increment;
    }
    
    SET_INDEXED_VERTEX(0);
    SET_INDEXED_VERTEX(last_index++);
    SET_UNTEXTURED_VERTEX(x1+radius+cos(angle)*radius, y2-radius+sin(angle)*radius, r, g, b, a);
    for(i = 1; i < verts_per_corner; i++)
    {
        SET_INDEXED_VERTEX(0);
        SET_INDEXED_VERTEX(last_index++);
        SET_UNTEXTURED_VERTEX(x1+radius+cos(angle)*radius, y2-radius+sin(angle)*radius, r, g, b, a);
        angle += corner_angle_increment;
    }
    
    SET_INDEXED_VERTEX(0);
    SET_INDEXED_VERTEX(last_index++);
    SET_UNTEXTURED_VERTEX(x1+radius+cos(angle)*radius, y1+radius+sin(angle)*radius, r, g, b, a);
    for(i = 1; i < verts_per_corner; i++)
    {
        SET_INDEXED_VERTEX(0);
        SET_INDEXED_VERTEX(last_index++);
        SET_UNTEXTURED_VERTEX(x1+radius+cos(angle)*radius, y1+radius+sin(angle)*radius, r, g, b, a);
        angle += corner_angle_increment;
    }
    
    // Last triangle
    SET_INDEXED_VERTEX(0);
    SET_INDEXED_VERTEX(last_index++);
    SET_INDEXED_VERTEX(1);
}

static void Polygon(GPU_Renderer* renderer, GPU_Target* target, unsigned int num_vertices, float* vertices, SDL_Color color)
{
    if(num_vertices < 3)
        return;
    
    int numSegments = 2*num_vertices;
    
    BEGIN_UNTEXTURED("GPU_Polygon", GL_LINES, num_vertices, numSegments);
    
    SET_UNTEXTURED_VERTEX(vertices[0], vertices[1], r, g, b, a);
    int last_index = 0;
    
    int i;
    for(i = 2; i < numSegments; i+=2)
    {
        SET_UNTEXTURED_VERTEX(vertices[i], vertices[i+1], r, g, b, a);
        last_index++;
        SET_INDEXED_VERTEX(last_index);  // Double the last one for the next line
    }
    SET_INDEXED_VERTEX(0);
}

static void PolygonFilled(GPU_Renderer* renderer, GPU_Target* target, unsigned int num_vertices, float* vertices, SDL_Color color)
{
    if(num_vertices < 3)
        return;
    
    int numSegments = 2*num_vertices;
    
    // Using a fan of triangles assumes that the polygon is convex
    BEGIN_UNTEXTURED("GPU_PolygonFilled", GL_TRIANGLES, num_vertices, 3 + (num_vertices-3)*3);
    
    // First triangle
    SET_UNTEXTURED_VERTEX(vertices[0], vertices[1], r, g, b, a);
    SET_UNTEXTURED_VERTEX(vertices[2], vertices[3], r, g, b, a);
    SET_UNTEXTURED_VERTEX(vertices[4], vertices[5], r, g, b, a);
    
    if(num_vertices > 3)
    {
        int last_index = 2;
        
        int i;
        for(i = 6; i < numSegments; i+=2)
        {
            SET_INDEXED_VERTEX(0);  // Start from the first vertex
            SET_INDEXED_VERTEX(last_index);  // Double the last one
            SET_UNTEXTURED_VERTEX(vertices[i], vertices[i+1], r, g, b, a);
            last_index++;
        }
    }
}

