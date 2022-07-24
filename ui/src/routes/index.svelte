
<script>
  import { Progress } from 'sveltestrap';
  import { onMount } from 'svelte';

  // ******* SHOW LEVEL ******** //
  let level = undefined;
  //level = [ 77, 22 ]
  onMount(async () => { 
    if (!!window.EventSource) {
      var source = new EventSource('/api/events');

      source.addEventListener('error', function(e) {
        if (e.target.readyState != EventSource.OPEN) {
          console.log("Events Disconnected");
        }
      }, false);

      source.addEventListener('message', function(e) {
        console.log("message", e.data);
      }, false);

      source.addEventListener('status', function(e) {
        try {
          let data = JSON.parse(e.data);          
          level = [];
          data.forEach((element, i) => {
            if ('level' in element) level[i] = element.level
          }); 
        } catch (error) {
          console.log(error);
          console.log("Error parsing status", e.data);          
        }
      }, false);
    }
  })

</script>

<svelte:head>
  <title>Sensor Status</title>
</svelte:head>

<div class="row">
{#if level == undefined}
    <div class="col">Requesting current level, please wait...</div>
{:else if level.length == 0}
    <div class="col">Please calibrate your Sensor...</div>
{:else}
    {#each {length: level.length} as _, i}
    <div class="col-sm-12">Current level of scale {(i+1)}</div>
    <div class="col-sm-12">
        <Progress animated value={level[i]} style="height: 5rem;">{level[i]}%</Progress>
    </div>
    {/each}
{/if}
</div>
