
<script>
  import { Progress } from 'sveltestrap';
  import { onMount } from 'svelte';

  // ******* SHOW LEVEL ******** //
  let level = undefined;
  //level = [ { id: ... },... ]
  onMount(async () => {
    // initial level
		const response = await fetch(`/api/level/current/all`, {
      headers: { "Content-type": "application/json" }
    }).catch(error => console.log(error));
    if(response.ok) {
      level = await response.json();
    } else {
      toast.push(`Error ${response.status} ${response.statusText}<br>Unable to request current level.`, variables.toast.error)
    }
  });

  onMount(async () => {
    // dynamic refreshing level
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
          level = JSON.parse(e.data);          
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
        <Progress animated value={level[i].level} style="height: 5rem;">{level[i].level}%<br>({(level[i].gasWeight/1000).toFixed(2)} Kg)</Progress>
    </div>
    {/each}
{/if}
</div>
