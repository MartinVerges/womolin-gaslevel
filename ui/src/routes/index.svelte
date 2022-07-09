
<script>
    import { variables } from '$lib/utils/variables';
    import { Progress, Button, Spinner } from 'sveltestrap';
    import { toast } from '@zerodevx/svelte-toast';
    import { onMount } from 'svelte';

    // ******* SHOW LEVEL ******** //
    let level = {};
    async function getCurrentLevel() {
        const response = await fetch(`/api/level/current/all`).catch(error => { throw new Error(`${error})`); });
        if(response.ok) level = await response.json();
        else {
            toast.push(`Error ${response.status} ${response.statusText}<br>Unable to request current level.`, variables.toast.error)
        }
    }

    let refreshInterval
    onMount(async () => { 
        getCurrentLevel()
        clearInterval(refreshInterval)
        refreshInterval = setInterval(getCurrentLevel, 5000)
    });
</script>

<svelte:head>
  <title>Sensor Status</title>
</svelte:head>

<div class="container">
    <div class="row">
    {#if level == undefined}
        <div class="col">Requesting current level, please wait...</div>
    {:else}
        {#each {length: level.length} as _, i}
        <div class="col-sm-12">Current level of scale {(i+1)}</div>
        <div class="col-sm-12">
            <Progress animated value={level[i]} style="height: 5rem;">{level[i]}%</Progress>
        </div>
        {/each}
    {/if}
    </div>
</div>
