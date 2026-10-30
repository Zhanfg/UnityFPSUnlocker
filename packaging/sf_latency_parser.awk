# SurfaceFlinger --latency parser. Col2 = presentation timestamp in ns.
# Ignores 0, duplicate/out of order timestamps and pending fences.
# Output is recent surface-presentation FPS, NOT game-engine or interpolated FPS.
BEGIN {
    frames=0; prev=0; period=0; first=0; last=0; bad=0; n=0;
}
NR==1 {
    if ($1 ~ /^[0-9]+$/ && $1+0>0) period=$1+0;
    next;
}
NF>=3 {
    stamp=$2;
    if (stamp !~ /^[0-9]+$/ || stamp=="9223372036854775807" ||
        stamp+0<=0 || stamp+0>=9.0e18) {bad++; next;}
    t=stamp+0;
    if (prev>0 && t<=prev) {bad++;next;}
    if (prev>0) {
        gap=t-prev;
        if (gap>0) {
            delta[n++]=gap;
        }
    }
    if (first==0) first=t;
    last=t;
    prev=t;
    frames++;
}
END {
    printf "refresh_period_ns=%.0f\n",period;
    printf "present_samples=%d\n",frames;
    printf "discarded_samples=%d\n",bad;
    if(frames<2 || n<1 || last<=first) {
        print "surface_present_fps=unavailable";
        exit 2;
    }
    fps=(frames-1)*1.0e9/(last-first);
    printf "surface_present_fps=%.3f\n",fps;
    # Selection sort avoids external sort; historical latency ring <=128 rows.
    for (i=0;i<n;i++) {
        min=i;
        for(j=i+1;j<n;j++)if(delta[j]<delta[min])min=j;
        v=delta[i];delta[i]=delta[min];delta[min]=v;
    }
    p50=int((n-1)*0.5);p95=int((n-1)*0.95);
    printf "frame_interval_p50_ms=%.3f\n",delta[p50]/1.0e6;
    printf "frame_interval_p95_ms=%.3f\n",delta[p95]/1.0e6;
    printf "capture_span_s=%.3f\n",(last-first)/1.0e9;
    if(period>0)printf "nominal_surface_refresh_hz=%.3f\n",1.0e9/period;
}
